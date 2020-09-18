#ifndef PROCESS_BASE_H_
#define PROCESS_BASE_H_

#include <cstdarg>
#include <mpl/mpl.hpp>

#pragma GCC diagnostic ignored "-Wformat-security"  // for log function

struct MessageBase;

class ProcessBase {
 private:
  int lamportClock = 0;
  const char* role;
  const mpl::communicator& communicator;
  std::vector<int> broadcastScope;
  std::list<std::pair<const MessageBase*, mpl::status>> messageBuffer;

  void setTimestamp(MessageBase& message) const;
  int getTimestamp(const MessageBase& message) const;
  void storeInBuffer(const MessageBase* message, const mpl::status& status);
  const MessageBase* fetchFromBuffer(mpl::status& status, int sourceRank,
                                     const std::vector<mpl::tag>& tags);

  template <typename T>
  bool receiveMultiTagHandle(
      int sourceRank, mpl::tag tag,
      std::unordered_map<int, std::function<void(const MessageBase*, const mpl::status&)>> messageHandlers) {
    T message{};
    const auto& status = communicator.recv(message, sourceRank, tag);
    if (messageHandlers.find((int)status.tag()) != messageHandlers.end()) {
      int timestamp = getTimestamp(message);
      lamportClock = std::max(lamportClock, timestamp) + 1;
      messageHandlers[(int)status.tag()](&message, status);
      return true;
    }
    storeInBuffer(new T(message), status);
    return false;
  }

 protected:
  const int rank;

  void setBroadcastScope(std::vector<int> recipientRanks);

  template <typename... Args>
  void log(char const* const format, Args const&... args) const {
    char buf[256];
    auto len1 = sprintf(buf, "%c[%d;%dm [Rank: %2d] [Clock: %3d] [%s] ", 27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamportClock, role);
    auto len2 = sprintf(buf+len1, format, args...);
    sprintf(buf+len1+len2, "%c[%d;%dm\n", 27,0,37);
    printf(buf);
  }

  template <typename T /* extends MessageBase */>
  void flush(mpl::tag tag) {
    T message;
    auto probe = communicator.iprobe(mpl::any_source, tag);
    while (probe.first) {
      auto status = probe.second;
      communicator.recv(message, status.source(), status.tag());
      probe = communicator.iprobe(mpl::any_source, tag);
    }
  }

  template <typename T /* extends MessageBase */>
  void send(T& message, int recipientRank, mpl::tag tag) {
    lamportClock++;
    setTimestamp(message);
    communicator.send(message, recipientRank, tag);
  }

  template <typename T /* extends MessageBase */>
  void broadcast(T& message, mpl::tag tag) {
    lamportClock++;
    setTimestamp(message);
    for (int recipientRank : broadcastScope) {
      if (recipientRank == rank) continue;
      communicator.send(message, recipientRank, tag);
    }
  }

  template <typename T /* extends MessageBase */>
  void broadcastVector(std::vector<T>& message, mpl::tag tag) {
    lamportClock++;
    for (int i = 0; i < message.size(); i++) {
      setTimestamp(message[i]);
    }
    for (int recipientRank : broadcastScope) {
      if (recipientRank == rank) continue;
      communicator.send(message.begin(), message.end(), recipientRank, tag);
    }
  }

  template <typename T /* extends MessageBase */>
  mpl::status receive(T& message, int sourceRank, mpl::tag tag) {
    mpl::status status;
    const MessageBase* bufferedMessage =
        fetchFromBuffer(status, sourceRank, std::vector<mpl::tag>{tag});
    if (bufferedMessage != nullptr) {
      message = *static_cast<const T*>(bufferedMessage);
      delete (bufferedMessage);
    } else {
      status = communicator.recv(message, sourceRank, tag);
    }
    int timestamp = getTimestamp(message);
    lamportClock = std::max(lamportClock, timestamp) + 1;
    return status;
  }

  template <typename T /* extends MessageBase */>
  mpl::status receiveAny(T& message, mpl::tag tag) {
    return receive(message, mpl::any_source, tag);
  }

  template <typename T /* extends MessageBase */>
  mpl::status receiveVector(std::vector<T>& message, int sourceRank, mpl::tag tag) {
    const mpl::status& probe = communicator.probe(sourceRank, tag);
    int size = probe.get_count<T>();
    if ((size == mpl::undefined) || (size == 0)) exit(EXIT_FAILURE);
    message.resize(size);
    mpl::status status =
        communicator.recv(message.begin(), message.end(), sourceRank, tag);
    int timestamp = getTimestamp(message[0]);
    lamportClock = std::max(lamportClock, timestamp) + 1;
    return status;
  }

  void receiveMultiTag(
      int sourceRank,
      std::unordered_map<int, std::function<void(const MessageBase*, const mpl::status&)>> messageHandlers);

 public:
  explicit ProcessBase(const mpl::communicator& communicator, const char* tag = "");
  virtual void run(int maxRounds) = 0;
};

#endif  // PROCESS_BASE_H_
