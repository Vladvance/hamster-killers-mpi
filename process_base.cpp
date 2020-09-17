#include "process_base.h"

#include "mpi_types.h"

ProcessBase::ProcessBase(const mpl::communicator& communicator, const char* tag)
    : communicator(communicator), rank(communicator.rank()), role(tag) {
  // initialize broadcast scope with all ranks
  broadcastScope.resize(communicator.size());
  std::iota(broadcastScope.begin(), broadcastScope.end(), 0);
}

void ProcessBase::setTimestamp(MessageBase& message) const {
  message.timestamp = lamportClock;
}

int ProcessBase::getTimestamp(const MessageBase& message) const {
  return message.timestamp;
}

void ProcessBase::setBroadcastScope(std::vector<int> recipientRanks) {
  broadcastScope = recipientRanks;
}

void ProcessBase::storeInBuffer(const MessageBase* message, const mpl::status& status) {
  messageBuffer.emplace_back(message, status);
}

const MessageBase* ProcessBase::fetchFromBuffer(mpl::status& status, int sourceRank, const std::vector<mpl::tag>& tags) {
  std::function<bool(std::pair<const MessageBase*, mpl::status>)> predicate;
  if (sourceRank == mpl::any_source) {
    predicate = [tags](std::pair<const MessageBase*, mpl::status> message) {
      return std::find(tags.begin(), tags.end(), message.second.tag()) != tags.end();
    };
  } else {
    predicate = [sourceRank, tags](std::pair<const MessageBase*, mpl::status> message) {
      return (message.second.source() == sourceRank) &&
             (std::find(tags.begin(), tags.end(), message.second.tag()) != tags.end());
    };
  }

  auto iterator =
      std::find_if(messageBuffer.begin(), messageBuffer.end(), predicate);
  if (iterator == messageBuffer.end()) {
    return nullptr;
  }
  auto message = iterator->first;
  status = iterator->second;
  messageBuffer.erase(iterator);
  return message;
}

void ProcessBase::receiveMultiTag(
    int sourceRank,
    std::unordered_map<int, std::function<void(const MessageBase*, const mpl::status&)>> messageHandlers) {
  std::vector<mpl::tag> tags(messageHandlers.size());
  for (const auto& element : messageHandlers) {
    tags.emplace_back((MessageType)element.first);
  }
  mpl::status status;
  const MessageBase* bufferedMessage =
      fetchFromBuffer(status, sourceRank, tags);
  if (bufferedMessage != nullptr) {
    int timestamp = getTimestamp(*bufferedMessage);
    lamportClock = std::max(lamportClock, timestamp) + 1;
    messageHandlers[(int)status.tag()](bufferedMessage, status);
    delete (bufferedMessage);
    return;
  }

  while (true) {
    const auto& probe = communicator.probe(mpl::any_source, mpl::tag::any());
    switch (static_cast<int>(probe.tag())) {
      case REQUEST_FOR_CONTRACT:
        if (receiveMultiTagHandle<RequestForContract>(sourceRank, probe.tag(), messageHandlers)) return;
        break;
      case REQUEST_FOR_ARMOR:
        if (receiveMultiTagHandle<RequestForArmor>(sourceRank, probe.tag(), messageHandlers)) return;
        break;
      case ALLOCATE_ARMOR:
        if (receiveMultiTagHandle<AllocateArmor>(sourceRank, probe.tag(), messageHandlers)) return;
        break;
      case CONTRACT_COMPLETED:
        if (receiveMultiTagHandle<ContractCompleted>(sourceRank, probe.tag(), messageHandlers)) return;
        break;
      case DELEGATE_PRIORITY:
        if (receiveMultiTagHandle<DelegatePriority>(sourceRank, probe.tag(), messageHandlers)) return;
        break;
      case SWAP:
        if (receiveMultiTagHandle<Swap>(sourceRank, probe.tag(), messageHandlers)) return;
        break;
      default:
        // Should never reach here
        log("Received unexpected message. Committing suicide.");
        return;
    }
  }
}
