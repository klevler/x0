#include <xzero/RaftServer.h>

namespace xzero {

// {{{ proto

// invoked by candidates to gather votes
struct RaftServer::VoteRequest {
  Term term;                // candidate's term
  Id candidateId;           // candidate requesting vode
  Index lastLogIndex;       // index of candidate's last log entry
  Term lastLogTerm;         // term of candidate's last log entry
};

struct RaftServer::VoteResponse {
  Term term;
  bool voteGranted;
};

// invoked by leader to replicate log entries; also used as heartbeat
struct RaftServer::AppendEntriesRequest {
  Term term;                     // leader's term
  Id leaderId;                   // so follower can redirect clients
  Index prevLogIndex;            // index of log entry immediately proceding new ones
  Term prevLogTerm;              // term of prevLogIndex entry
  std::vector<LogEntry> entries; // log entries to store (empty for heartbeat; may send more than one for efficiency)
  Index leaderCommit;            // leader's commitIndex
};

struct RaftServer::AppendEntriesResponse {
  Term term;
  bool success;
};

// Invoked by leader to send chunks of a snapshot to a follower.
// Leaders always send chunks in order.
struct RaftServer::InstallSnapshotRequest {
  Term term;
  Id leaderId;
  Index lastIncludedIndex;
  Term lastIncludedTerm;
  size_t offset;
  std::vector<uint8_t> data;
  bool done;
};

struct RaftServer::InstallSnapshotResponse {
  Term term;
};
// }}} proto

RaftServer::RaftServer(Id id,
                       Storage* storage,
                       Discovery* discovery,
                       Transport* transport,
                       StateMachine* sm)
      : RaftServer(id, storage, discovery, transport, sm,
                   500_milliseconds,
                   300_milliseconds,
                   500_milliseconds) {
}

RaftServer::RaftServer(Id id,
                       Storage* storage,
                       Discovery* discovery,
                       Transport* transport,
                       StateMachine* sm,
                       Duration heartbeatTimeout,
                       Duration electionTimeout,
                       Duration commitTimeout)
    : id_(id),
      storage_(storage),
      discovery_(discovery),
      transport_(transport),
      stateMachine_(sm),
      state_(FOLLOWER),
      heartbeatTimeout_(heartbeatTimeout),
      electionTimeout_(electionTimeout),
      commitTimeout_(commitTimeout),
      currentTerm_(0),
      votedFor_(),
      commitIndex_(0),
      lastApplied_(0),
      nextIndex_(),
      matchIndex_()
  {
}

RaftServer::~RaftServer() {
}

// {{{ RaftServer: receiver API (invoked by Transport on receiving messages)
void RaftServer::receive(Id from, const VoteRequest& message) {
  // TODO
}

void RaftServer::receive(Id from, const VoteResponse& message) {
  // TODO
}

void RaftServer::receive(Id from, const AppendEntriesRequest& message) {
  // TODO
}

void RaftServer::receive(Id from, const AppendEntriesResponse& message) {
  // TODO
}

void RaftServer::receive(Id from, const InstallSnapshotRequest& message) {
  // TODO
}

void RaftServer::receive(Id from, const InstallSnapshotResponse& message) {
  // TODO
}
// }}}
// {{{ RaftServer::StaticDiscovery
void RaftServer::StaticDiscovery::add(Id id) {
  members_.emplace_back(id);
}

std::vector<RaftServer::Id> RaftServer::StaticDiscovery::listMembers() {
  return members_;
}
// }}}
// {{{ LocalTransport
RaftServer::LocalTransport::LocalTransport(Id localId)
    : localId_(localId) {
}

void RaftServer::LocalTransport::send(Id target, const VoteRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const VoteResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const AppendEntriesRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const AppendEntriesResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const InstallSnapshotRequest& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}

void RaftServer::LocalTransport::send(Id target, const InstallSnapshotResponse& message) {
  auto i = peers_.find(target);
  if (i != peers_.end()) {
    i->second->receive(localId_, message);
  }
}
// }}}
// {{{ RaftServer::MemoryStore
RaftServer::MemoryStore::MemoryStore()
    : id_(),
      currentTerm_(),
      log_(),
      snapshottedTerm_(),
      snapshottedIndex_(),
      snapshotData_() {

  // log with index 0 is invalid. logs start with index 1
  log_.push_back(LogEntry());
}

bool RaftServer::MemoryStore::saveCandidateId(const Id& id) {
  id_ = id;
  return true;
}

void RaftServer::MemoryStore::loadCandidateId(Id* id) {
  *id = id_;
}

bool RaftServer::MemoryStore::saveTerm(Term currentTerm) {
  currentTerm_ = currentTerm;
  return true;
}

void RaftServer::MemoryStore::loadTerm(Term* currentTerm) {
  *currentTerm = currentTerm_;
}

bool RaftServer::MemoryStore::appendLogEntry(const LogEntry& log) {
  if (log_.size() != log.index()) {
    abort();
  }
  log_.push_back(log);
  return true;
}

void RaftServer::MemoryStore::loadLogEntry(Index index, LogEntry* log) {
  *log = log_[index];
}

bool RaftServer::MemoryStore::saveSnapshotBegin(Term currentTerm, Index lastIndex) {
  snapshottedTerm_ = currentTerm;
  snapshottedIndex_ = lastIndex;
  return true;
}

bool RaftServer::MemoryStore::saveSnapshotChunk(const uint8_t* data, size_t length) {
  // most ugly art to append a data block to another
  for (size_t i = 0; i < length; ++i) {
    snapshotData_.push_back(data[i]);
  }
  return true;
}

bool RaftServer::MemoryStore::saveSnapshotEnd() {
  return true;
}

bool RaftServer::MemoryStore::loadSnapshotBegin(Term* currentTerm, Index* lastIndex) {
  return false;
}

bool RaftServer::MemoryStore::loadSnapshotChunk(std::vector<uint8_t>* chunk) {
  return false;
}
// }}}

} // namespace xzero

