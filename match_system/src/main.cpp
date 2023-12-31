// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include <iostream>
#include <queue>
#include <mutex>
#include <vector>
#include <unistd.h>
#include <condition_variable>

#include "match_server/Match.h"
#include "save_client/Save.h"
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/TToString.h>

using namespace std;
using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace  ::match_service;
using namespace  ::save_service;

/* Define element in MQ */
struct Task {
	User user;
	string type;
};

/* Define MQ */
struct MessageQueue {
	queue<Task> q;
	/* A lock to guarantee mutual exclusion of queue operation */
	mutex m;
	/* A conditional variable for communicating between producer and comsumer*/
	condition_variable cv;

} message_queue;

/* Create a user pool */
class Pool {
	private:
	/* User pool */
	vector<User> users;
	/* Wait time */
	vector<int> wt;
	public:
	void add(User user) {
		users.push_back(user);
		wt.push_back(0);
	}
	void remove(User user) {
		for (uint32_t i = 0; i < users.size(); i ++) {
			if (users[i].id == user.id) {
				users.erase(users.begin() + i);
				wt.erase(wt.begin() + i);
				break;
			}
		}
	}
	void save_result(int a, int b) {
		printf("Match Result: %d %d\n", a, b);

		/* save client start */
		std::shared_ptr<TTransport> socket(new TSocket("123.57.47.211", 9090));
		std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
		std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
		SaveClient client(protocol);

		try {
			transport->open();
			/* Real logic */
			/* call save_data defined in save_service.thrift */
			client.save_data("acs_158", "28bd356e", a, b);

			transport->close();
		} catch (TException& tx) {
			cout << "ERROR: " << tx.what() << endl;
		}
		/* save client end */
	}
	/* check if user with id i and j can be matched */
	bool check_match(uint32_t i, uint32_t j) {
		auto a = users[i], b = users[j];
		int dt = abs(a.score - b.score);

		int a_df = wt[i] * 50;
		int b_df = wt[j] * 50;

		return dt <= a_df && dt <= b_df;
	}
	void match() {
		/* Increment number of rounds */
		for (uint32_t i = 0; i < wt.size(); i ++) {
			wt[i] ++;
		}
		while (users.size() > 1) {
			bool flag = false;
			for (uint32_t i = 0; i < users.size(); i ++) {
				for (uint32_t j = i + 1; j < users.size(); j ++) {
					if (check_match(i, j)) {
						users.erase(users.begin() + j);
						users.erase(users.begin() + i);
						wt.erase(wt.begin() + j);
						wt.erase(wt.begin() + i);
						save_result(users[i].id, users[j].id);
						flag = true;
						break;
					}
				}
				if (flag) break;
			}
			/* If no one match, then break, if one pair matches, continue to see */
			if (!flag) break;
		}
	}
} pool;



class MatchHandler : virtual public MatchIf {
public:
	MatchHandler() {
		// Your initialization goes here
	}

	int32_t add_user(const User& user, const std::string& info) {
		// Your implementation goes here
		printf("add_user\n");

		/* Try to get the lock of MQ */
		unique_lock<mutex> lck(message_queue.m);

		/* Create a task and push it to MQ */
		message_queue.q.push({ user, "add" });

		/* Notify the consumer for consuming task */
		message_queue.cv.notify_all();

		/* Release the lock */
		return 0;

	}

	int32_t remove_user(const User& user, const std::string& info) {
		// Your implementation goes here
		printf("remove_user\n");

		unique_lock<mutex> lck(message_queue.m);

		message_queue.q.push({ user, "remove" });

		message_queue.cv.notify_all();

		return 0;
	}

};

/* Consumer function*/
void consume_task() {

	while (true) {

		/* Try to get the lock */
		unique_lock<mutex> lck(message_queue.m);

		/* If MQ is empty*/
		if (message_queue.q.empty()) {
			/* unlock */
			lck.unlock();
			/* Match in every second */
			pool.match();
			sleep(1);
		}
		/* If MQ is not empty*/
		else {
			/* Get the first task */
			auto task = message_queue.q.front();
			message_queue.q.pop();

			/* Release the lock because next operation is not relevant to queue operation */
			lck.unlock();

			/* do task */
			if (task.type == "add") pool.add(task.user);
			else if (task.type == "remove") pool.remove(task.user);
		}
	}
}

/*
  MatchIfFactory is code generated.
  MatchCloneFactory is useful for getting access to the server side of the
  transport.  It is also useful for making per-connection state.  Without this
  CloneFactory, all connections will end up sharing the same handler instance.
*/
class MatchCloneFactory : virtual public MatchIfFactory {
public:
	~MatchCloneFactory() override = default;
	MatchIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
	{
		std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);
		return new MatchHandler;
	}
	void releaseHandler(MatchIf *handler) override {
		delete handler;
	}
};

int main(int argc, char **argv) {
	int port = 9090;
	::std::shared_ptr<MatchHandler> handler(new MatchHandler());
	::std::shared_ptr<TProcessor> processor(new MatchProcessor(handler));
	::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
	::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
	::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

	TThreadedServer server(
		std::make_shared<MatchProcessorFactory>(std::make_shared<MatchCloneFactory>()),
		std::make_shared<TServerSocket>(9090), //port
		std::make_shared<TBufferedTransportFactory>(),
		std::make_shared<TBinaryProtocolFactory>());

	cout << "Start Match Serivce.." << endl;

	/* Start consumber task */
	thread matching_thread(consume_task);

	server.serve();
	return 0;
}

