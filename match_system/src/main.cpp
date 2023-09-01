// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include <iostream>
#include <queue>
#include <mutex>
#include <vector>
#include <condition_variable>

#include "match_server/Match.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

using namespace std;
using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace  ::match_service;

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
		vector<User> users;
	public:
		void add(User user) {
			users.push_back(user);
		}
		void remove(User user) {
			for (uint32_t i = 0; i < users.size(); i ++) {
				if (users[i].id == user.id) {
					users.erase(users.begin() + i);
				}
			}
		}
		void save_result(int a, int b) {
			printf("Match Result: %d %d", a, b);
		}
		void match() {
			while (users.size() > 1) {
				auto a = users[0], b = users[1];
				users.erase(users.begin());
				users.erase(users.begin());

				save_result(a.id, b.id);
			}
		}
};



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
			/* Wait until there's task */
			/* Lock lck is released temporarily*/
			message_queue.cv.wait(lck);
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

			pool.match();

		}
	}
}

int main(int argc, char **argv) {
	int port = 9090;
	::std::shared_ptr<MatchHandler> handler(new MatchHandler());
	::std::shared_ptr<TProcessor> processor(new MatchProcessor(handler));
	::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
	::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
	::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

	TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);

	cout << "Start Match Serivce.." << endl;

	/* Start consumber task */
	thread matching_thread(consume_task);

	server.serve();
	return 0;
}

