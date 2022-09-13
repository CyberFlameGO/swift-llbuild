//===-- FileInfo.cpp ------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "llbuild/Basic/FileInfo.h"

#include "llbuild/Basic/Stat.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <cstdio>
#include <cerrno>

#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include "llvm/Support/MD5.h"
#endif

using namespace llbuild;
using namespace llbuild::basic;

bool FileInfo::isDirectory() const {
  return (mode & S_IFDIR) != 0;
}

/// Get the information to represent the state of the given node in the file
/// system.
FileInfo FileInfo::getInfoForPath(const std::string& path, bool asLink) {
  FileInfo result;

  sys::StatStruct buf;
  auto statResult =
    asLink ? sys::lstat(path.c_str(), &buf) : sys::stat(path.c_str(), &buf);
  if (statResult != 0) {
    memset(&result, 0, sizeof(result));
    assert(result.isMissing());
    return result;
  }

  result.device = buf.st_dev;
  result.inode = buf.st_ino;
  result.mode = buf.st_mode;
  result.size = buf.st_size;
#if defined(__APPLE__)
  auto seconds = buf.st_mtimespec.tv_sec;
  auto nanoseconds = buf.st_mtimespec.tv_nsec;
#elif defined(_WIN32)
  auto seconds = buf.st_mtime;
  auto nanoseconds = 0;
#else
  auto seconds = buf.st_mtim.tv_sec;
  auto nanoseconds = buf.st_mtim.tv_nsec;
#endif
  result.modTime.seconds = seconds;
  result.modTime.nanoseconds = nanoseconds;

  // Enforce we never accidentally create our sentinel missing file value.
  if (result.isMissing()) {
    result.modTime.nanoseconds = 1;
    assert(!result.isMissing());
  }

  return result;
}

FileChecksum FileChecksum::getChecksumForPath(const std::string& path) {
  FileChecksum result;

  FileInfo fileInfo = FileInfo::getInfoForPath(path);
  if (fileInfo.isMissing()) {
    for(int i=0; i<32; i++) {
      result.bytes[i] = 0;
    }
  } else if (fileInfo.isDirectory()) {
    result.bytes[0] = 1;
  } else {
    FILE *file = NULL;
    file = std::fopen(path.c_str(), "rb");
    char buffer[4*4096];
    size_t bytesRead = 0;

#ifdef __APPLE__
    CC_SHA256_CTX ctx = {};
    unsigned char temp_digest[CC_SHA256_DIGEST_LENGTH] = { 0 };
    CC_SHA256_Init( &ctx );

    if (file != NULL) {
      while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        CC_SHA256_Update(&ctx, buffer, bytesRead );
      }
      fclose(file);
    } else {
      abort();
    }

    CC_SHA256_Final( temp_digest, &ctx );
    std::copy(temp_digest, temp_digest+CC_SHA256_DIGEST_LENGTH, result.bytes);
#else
    llvm::MD5 hasher;

    if (file != NULL) {
      while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        const char* buffer2 = buffer;
        hasher.update(StringRef(buffer2, bytesRead));
      }
      fclose(file);
    } else {
      abort();
    }
  }
  llvm::MD5::MD5Result output;
  hasher.final(output);
  std::copy(output.Bytes.begin(), output.Bytes.end(), result.bytes);
#endif
  }

  return result;
}
