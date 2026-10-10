#pragma once

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace aq::logger::detail {

using DirectoryStat = int (*)(const char *, struct stat *);
using DirectoryMkdir = int (*)(const char *, mode_t);

// The caller owns filesystem/bus serialization. FatFs rejects mkdir at its
// mounted root with EINVAL rather than EEXIST; inspect existing paths first.
inline bool ensure_directory(const char *path, DirectoryStat inspect = ::stat,
                             DirectoryMkdir create = ::mkdir) {
  struct stat info{};
  if (inspect(path, &info) == 0) {
    if (S_ISDIR(info.st_mode))
      return true;
    errno = ENOTDIR;
    return false;
  }
  if (errno != ENOENT)
    return false;
  if (create(path, 0700) == 0)
    return true;
  if (errno != EEXIST)
    return false;
  // Do not accept a regular file or a failed lookup after an EEXIST race.
  if (inspect(path, &info) != 0)
    return false;
  if (S_ISDIR(info.st_mode))
    return true;
  errno = ENOTDIR;
  return false;
}

// Bounded, allocation-free archive traversal. IO supplies open/read/inspect/
// close with any required bus locking. A null read is EOF only when errno is
// zero; FatFs otherwise reports an SD read failure through that same result.
// Fail the entire snapshot on any inaccessible entry, rather than publishing
// the successfully visited prefix as a complete archive.
template <class IO, class Visitor>
bool walk_directory(IO &io, const char *base, const char *relative,
                    unsigned depth, Visitor &visit) {
  if (depth > 6) {
    errno = EOVERFLOW;
    return false;
  }
  char path[416];
  int length = std::snprintf(path, sizeof(path), "%s/%s", base, relative);
  if (length < 0 || static_cast<std::size_t>(length) >= sizeof(path)) {
    errno = ENAMETOOLONG;
    return false;
  }
  auto *directory = io.open(path);
  if (!directory)
    return false;
  bool complete = true;
  for (;;) {
    errno = 0;
    const auto *entry = io.read(directory);
    if (!entry) {
      complete = errno == 0;
      break;
    }
    if (entry->d_name[0] == '.')
      continue;
    char name[384];
    length = std::snprintf(name, sizeof(name), "%s%s%s", relative,
                           *relative ? "/" : "", entry->d_name);
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(name)) {
      errno = ENAMETOOLONG;
      complete = false;
      break;
    }
    length = std::snprintf(path, sizeof(path), "%s/%s", base, name);
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(path)) {
      errno = ENAMETOOLONG;
      complete = false;
      break;
    }
    struct stat info{};
    if (io.inspect(path, &info) != 0 ||
        !(S_ISDIR(info.st_mode)
              ? walk_directory(io, base, name, depth + 1, visit)
              : visit(name, info))) {
      complete = false;
      break;
    }
  }
  const int failure = errno;
  if (io.close(directory) != 0)
    complete = false;
  if (failure)
    errno = failure;
  return complete;
}

} // namespace aq::logger::detail
