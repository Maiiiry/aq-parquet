#include "aq_logger_directory.h"

#include <cassert>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

namespace {
enum class Scenario {
  MountedRoot,
  ExistingFile,
  Missing,
  Denied,
  RaceDir,
  RaceFile
};
Scenario scenario;
unsigned inspections = 0;
unsigned creations = 0;

int inspect(const char *path, struct stat *info) {
  assert(std::strcmp(path, "/sd") == 0);
  ++inspections;
  if (scenario == Scenario::Denied) {
    errno = EACCES;
    return -1;
  }
  if (scenario == Scenario::Missing ||
      ((scenario == Scenario::RaceDir || scenario == Scenario::RaceFile) &&
       inspections == 1)) {
    errno = ENOENT;
    return -1;
  }
  info->st_mode =
      scenario == Scenario::ExistingFile || scenario == Scenario::RaceFile
          ? S_IFREG
          : S_IFDIR;
  return 0;
}

int create(const char *path, mode_t mode) {
  assert(std::strcmp(path, "/sd") == 0 && mode == 0700);
  ++creations;
  if (scenario == Scenario::MountedRoot) {
    // ESP-IDF FatFs mkdir('/') differs from POSIX mkdir of an existing dir.
    errno = EINVAL;
    return -1;
  }
  if (scenario == Scenario::RaceDir || scenario == Scenario::RaceFile) {
    errno = EEXIST;
    return -1;
  }
  assert(scenario == Scenario::Missing);
  return 0;
}

bool check(Scenario value) {
  scenario = value;
  inspections = creations = 0;
  errno = ERANGE; // Successful stat/mkdir need not clear a prior errno.
  return aq::logger::detail::ensure_directory("/sd", inspect, create);
}

struct FakeArchiveIO {
  struct Cursor {
    std::string path;
    unsigned index = 0;
    bool open = false;
  } cursors[2];
  struct dirent entry{};
  std::string fail_open, fail_read, fail_stat, fail_close;
  unsigned opened = 0, closed = 0;

  Cursor *open(const char *path) {
    if (path == fail_open) {
      errno = EIO;
      return nullptr;
    }
    const unsigned index = std::strcmp(path, "/sd/") == 0 ? 0 : 1;
    auto &cursor = cursors[index];
    assert(!cursor.open);
    cursor = {path, 0, true};
    ++opened;
    return &cursor;
  }
  const struct dirent *read(Cursor *cursor) {
    const char *names[2] = {"first.parquet", "day"};
    const bool root = cursor == &cursors[0];
    if (cursor->index == (root ? 2U : 1U)) {
      // The failed SD read happens after a valid file was returned.
      if (cursor->path == fail_read)
        errno = EIO;
      return nullptr;
    }
    std::snprintf(entry.d_name, sizeof(entry.d_name), "%s",
                  root ? names[cursor->index] : "latest.parquet");
    ++cursor->index;
    return &entry;
  }
  int inspect(const char *path, struct stat *info) {
    if (path == fail_stat) {
      errno = EIO;
      return -1;
    }
    info->st_mode = std::strcmp(path, "/sd/day") == 0 ? S_IFDIR : S_IFREG;
    // A successful POSIX call need not clear a previous unrelated errno.
    errno = ERANGE;
    return 0;
  }
  int close(Cursor *cursor) {
    assert(cursor->open);
    cursor->open = false;
    ++closed;
    if (cursor->path == fail_close) {
      errno = EIO;
      return -1;
    }
    return 0;
  }
};

void check_archive_walk() {
  using aq::logger::detail::walk_directory;
  for (unsigned failure = 0; failure < 6; ++failure) {
    FakeArchiveIO io;
    switch (failure) {
    case 1:
      io.fail_read = "/sd/day";
      break;
    case 2:
      io.fail_read = "/sd/";
      break;
    case 3:
      io.fail_open = "/sd/day";
      break;
    case 4:
      io.fail_stat = "/sd/day/latest.parquet";
      break;
    case 5:
      io.fail_close = "/sd/day";
      break;
    }
    std::vector<std::string> files;
    auto visit = [&](const char *name, const struct stat &) {
      files.emplace_back(name);
      return true;
    };
    errno = EACCES;
    const bool complete = walk_directory(io, "/sd", "", 0, visit);
    assert(complete == (failure == 0));
    if (failure)
      assert(errno == EIO);
    assert(io.opened == io.closed); // including recursive error unwinding
    assert(files.size() == (failure == 3 || failure == 4 ? 1U : 2U));
    assert(files[0] == "first.parquet");
    if (files.size() == 2)
      assert(files[1] == "day/latest.parquet");
  }
  FakeArchiveIO io;
  auto visit = [](const char *, const struct stat &) {
    errno = ENAMETOOLONG;
    return false;
  };
  assert(!walk_directory(io, "/sd", "", 0, visit));
  assert(errno == ENAMETOOLONG && io.opened == io.closed);
  assert(!walk_directory(io, "/sd", "", 7, visit));
  assert(errno == EOVERFLOW);
  const std::string long_base(416, 'a');
  assert(!walk_directory(io, long_base.c_str(), "", 0, visit));
  assert(errno == ENAMETOOLONG);
}
} // namespace

int main() {
  assert(check(Scenario::MountedRoot));
  assert(inspections == 1 && creations == 0);
  assert(!check(Scenario::ExistingFile));
  assert(errno == ENOTDIR && creations == 0);
  assert(check(Scenario::Missing));
  assert(creations == 1);
  assert(!check(Scenario::Denied));
  assert(errno == EACCES && creations == 0);
  assert(check(Scenario::RaceDir));
  assert(inspections == 2 && creations == 1);
  assert(!check(Scenario::RaceFile));
  assert(errno == ENOTDIR && inspections == 2 && creations == 1);
  check_archive_walk();
}
