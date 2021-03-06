// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
// This source code is licensed under both the GPLv2 (found in the
// COPYING file in the root directory) and Apache 2.0 License
// (found in the LICENSE.Apache file in the root directory).

// WAL related classes used in VersionEdit and VersionSet.
// Modifications to WalAddition and WalDeletion may need to update
// VersionEdit and its related tests.

#pragma once

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "logging/event_logger.h"
#include "port/port.h"
#include "rocksdb/rocksdb_namespace.h"

namespace ROCKSDB_NAMESPACE {

class JSONWriter;
class Slice;
class Status;

using WalNumber = uint64_t;

// Metadata of a WAL.
class WalMetadata {
 public:
  WalMetadata() = default;

  explicit WalMetadata(uint64_t synced_size_bytes)
      : synced_size_bytes_(synced_size_bytes) {}

  bool IsClosed() const { return closed_; }

  void SetClosed() { closed_ = true; }

  bool HasSyncedSize() const { return synced_size_bytes_ != kUnknownWalSize; }

  void SetSyncedSizeInBytes(uint64_t bytes) { synced_size_bytes_ = bytes; }

  uint64_t GetSyncedSizeInBytes() const { return synced_size_bytes_; }

 private:
  // The size of WAL is unknown, used when the WAL is not synced yet or is
  // empty.
  constexpr static uint64_t kUnknownWalSize = port::kMaxUint64;

  // Size of the most recently synced WAL in bytes.
  uint64_t synced_size_bytes_ = kUnknownWalSize;

  // Whether the WAL is closed.
  bool closed_ = false;
};

// These tags are persisted to MANIFEST, so it's part of the user API.
enum class WalAdditionTag : uint32_t {
  // Indicates that there are no more tags.
  kTerminate = 1,
  // Synced Size in bytes.
  kSyncedSize = 2,
  // Whether the WAL is closed.
  kClosed = 3,
  // Add tags in the future, such as checksum?
};

// Records the event of adding a WAL in VersionEdit.
class WalAddition {
 public:
  WalAddition() : number_(0), metadata_() {}

  explicit WalAddition(WalNumber number) : number_(number), metadata_() {}

  WalAddition(WalNumber number, WalMetadata meta)
      : number_(number), metadata_(std::move(meta)) {}

  WalNumber GetLogNumber() const { return number_; }

  const WalMetadata& GetMetadata() const { return metadata_; }

  void EncodeTo(std::string* dst) const;

  Status DecodeFrom(Slice* src);

  std::string DebugString() const;

 private:
  WalNumber number_;
  WalMetadata metadata_;
};

std::ostream& operator<<(std::ostream& os, const WalAddition& wal);
JSONWriter& operator<<(JSONWriter& jw, const WalAddition& wal);

using WalAdditions = std::vector<WalAddition>;

// Records the event of deleting/archiving a WAL in VersionEdit.
class WalDeletion {
 public:
  WalDeletion() : number_(0) {}

  explicit WalDeletion(WalNumber number) : number_(number) {}

  WalNumber GetLogNumber() const { return number_; }

  void EncodeTo(std::string* dst) const;

  Status DecodeFrom(Slice* src);

  std::string DebugString() const;

 private:
  WalNumber number_;
};

std::ostream& operator<<(std::ostream& os, const WalDeletion& wal);
JSONWriter& operator<<(JSONWriter& jw, const WalDeletion& wal);

using WalDeletions = std::vector<WalDeletion>;

// Used in VersionSet to keep the current set of WALs.
//
// When a WAL is created, closed, deleted, or archived,
// a VersionEdit is logged to MANIFEST and
// the WAL is added to or deleted from WalSet.
//
// Not thread safe, needs external synchronization such as holding DB mutex.
class WalSet {
 public:
  // Add WAL(s).
  // If the WAL is closed,
  // then there must be an existing unclosed WAL,
  // otherwise, return Status::Corruption.
  // Can happen when applying a VersionEdit or recovering from MANIFEST.
  Status AddWal(const WalAddition& wal);
  Status AddWals(const WalAdditions& wals);

  // Delete WAL(s).
  // The WAL to be deleted must exist and be closed, otherwise,
  // return Status::Corruption.
  // Can happen when applying a VersionEdit or recovering from MANIFEST.
  Status DeleteWal(const WalDeletion& wal);
  Status DeleteWals(const WalDeletions& wals);

  // Resets the internal state.
  void Reset();

  const std::map<WalNumber, WalMetadata>& GetWals() const { return wals_; }

 private:
  std::map<WalNumber, WalMetadata> wals_;
};

}  // namespace ROCKSDB_NAMESPACE
