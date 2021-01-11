#include <iostream>
#include <thread>
#include <mutex>
#include <random>
#include <map>
#include <iostream>
#include <fstream> // ifstream urandom("/dev/urandom", ios::in|ios::binary);
#include <stdlib.h>
template <class T>
class Node {
public:
    explicit Node(T d) : data(d) {};

    void setPrev(std::shared_ptr<Node> n) {
        prev = n;
    }

    void setNext(std::shared_ptr<Node> n) {
        next = n;
    }

    std::shared_ptr<Node> getNext() {
        return next;
    }

    std::shared_ptr<Node> getPrev() {
        return prev;
    }

    T getData() {
        return data;
    }

    std::mutex m;

private:
    T data;
    std::shared_ptr<Node> prev;
    std::shared_ptr<Node> next;

};

template <class T>
class List {
public:
    List() {
        head = nullptr;
        tail = nullptr;
        size = 0;
    }

    int append(T newData) {
        if (newData.empty()) {
            return -1;
        }

        auto newNode = std::make_shared<Node<T>>(newData);

        if (size == 0) {
            head = newNode;
        } else {
            auto guard = std::unique_lock<std::mutex>(tail->m);
            tail->setNext(newNode);
        }
        newNode->setPrev(tail);
        tail = newNode;
        size += 1;

        return 0;
    }

    int prepend(T newData) {
        if (newData.empty()) {
            return -1;
        }

        auto newNode = std::make_shared<Node<T>>(newData);

        if (size == 0) {
            tail = newNode;
        } else {
            auto guard = std::unique_lock<std::mutex>(head->m);
            head->setPrev(newNode);
        }
        newNode->setNext(head);
        head = newNode;
        size += 1;

        return 0;
    }

    int insert(T newData) {
        if (newData.empty()) {
            return -1;
        }

        auto newNode = std::make_shared<Node<T>>(newData);

        auto t = std::this_thread::get_id();
        auto current = threadPos[t];
        auto next = current->getNext();
        if (next) {
            auto guard = std::unique_lock<std::mutex>(next->m);
            next->setPrev(newNode);
        }
        if (current == tail) {
            tail = newNode;
        }
        current->setNext(newNode);

        size += 1;

        return 0;
    }

    int getSize() {
        return size;
    }

    int removeNode() {
        auto t = std::this_thread::get_id();
        auto n = threadPos[t];

        threadPos[t]->m.unlock();

        std::shared_ptr<Node<T>> prev = n->getPrev();
        std::shared_ptr<Node<T>> next = n->getNext();

        std::unique_lock<std::mutex> lock_this(n->m, std::defer_lock);

        if (prev == nullptr && next == nullptr) { // n is only node in the list
            lock_this.lock();
        } else if (prev == nullptr) {             // n is head
            auto lock_next = std::unique_lock<std::mutex>(next->m, std::defer_lock);
            std::lock(lock_this, lock_next);
        } else if (next == nullptr) {             // n is tail
            auto lock_prev = std::unique_lock<std::mutex>(prev->m, std::defer_lock);
            std::lock(lock_this, lock_prev);
        } else {                                  // n is in the middle
            auto lock_prev = std::unique_lock<std::mutex>(prev->m, std::defer_lock);
            auto lock_next = std::unique_lock<std::mutex>(next->m, std::defer_lock);
            std::lock(lock_this, lock_prev, lock_next);
        }

        if (prev == nullptr) {
            head = next;
        } else {
            prev->setNext(next);
        }
        if (next == nullptr) {
            tail = prev;
        } else {
            next->setPrev(prev);
        }
        threadPos[t] = nullptr;
        size -= 1;

        return 0;
    }


    T moveForward() {
        auto t = std::this_thread::get_id();
        auto next = threadPos[t]->getNext();
        if (next) {
            next->m.lock();
            threadPos[t]->m.unlock();
            threadPos[t] = next;
            return threadPos[t]->getData();
        } else {
            return std::string();
        }
    }

    T moveBack() {
        auto t = std::this_thread::get_id();
        auto prev = threadPos[t]->getPrev();
        if (prev) {
            prev->m.lock();
            threadPos[t]->m.unlock();
            threadPos[t] = prev;
            return threadPos[t]->getData();
        } else {
            return std::string();
        }
    }

    T goToHead() {
        auto t = std::this_thread::get_id();
        if (head) {
            head->m.lock();
        }
        if (threadPos[t]) {
            threadPos[t]->m.unlock();
        }
        threadPos[t] = head;
        if (threadPos[t]) {
            return threadPos[t]->getData();
        } else {
            return std::string();
        }
    }

    T goToTail() {
        auto t = std::this_thread::get_id();
        if (tail) {
            tail->m.lock();
        }
        if (threadPos[t]) {
            threadPos[t]->m.unlock();
        }
        threadPos[t] = tail;
        if (threadPos[t]) {
            return threadPos[t]->getData();
        } else {
            return std::string();
        }
    }

    T getCurrentData() {
        auto t = std::this_thread::get_id();
        return threadPos[t];
    }

    int clearPosition() {
        auto t = std::this_thread::get_id();
        auto current = threadPos[t];
        if (current) {
            threadPos[t]->m.unlock();
            threadPos[t] = nullptr;
        }

        return 0;
    }
    std::shared_ptr<Node<T>> head;
    std::shared_ptr<Node<T>> tail;
private:

    int size;
    std::map<std::thread::id, std::shared_ptr<Node<T>>> threadPos;
};

void task1(List<std::string>& list) {
    // Traverse the list in order, concatenate all strings of nodes in list in order into one string,
    // which is output to std::cout when entire double-linked list has been traversed. Keep repeating
    // this over and over while there are nodes in the list, and then stop when there are none.

    while (list.getSize() > 0) {
        std::string allWords;
        auto nextData = list.goToHead();
        while (!nextData.empty()) {
            allWords += nextData;
            nextData = list.moveForward();
        }
        list.clearPosition();
        std::cout << allWords << "\n";
    }
}

void task2(List<std::string>& list) {
    // Every 1 second, randomly select a node from nodes in the list right now, and delete it.
    // Keep doing this every second while there are nodes in the list, then stop when there are none.

    std::random_device rd;
    std::default_random_engine e{rd()};

    while (list.getSize() > 0) {
        std::uniform_int_distribution<int> distNode{0, list.getSize() - 1};
        int nodeNum = distNode(e);
        list.goToHead();
        for (int i = 0; i < nodeNum; i++) {
            auto dat = list.moveForward();
        }
        list.removeNode();
        list.clearPosition();

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void task3(List<std::string>& list,bool f){
    unsigned int count = 0;
    unsigned int i = 0;
    if (f){
        while(list.getSize() > 0){
            auto nextData = list.goToHead();
            while (!nextData.empty()) {
                int check = atoi(nextData.c_str());
                while (check != 0){

                    count += check & 1;
                    check = check >> 1;
                }
                i++;
                list.removeNode();
            }
        }
        count = sizeof(int)*8*i - count;
        std::cout << count << " " << i << "\n";

    }
    else {
        while(list.getSize() > 0){
            auto nextData = list.goToTail();
            while (!nextData.empty()) {
                int check = atoi(nextData.c_str());
                while (check != 0){

                    count += check & 1;
                    check = check >> 1;
                }
                i++;
                list.removeNode();
            }
        }
        count = sizeof(int)*8*i - count;
        std::cout << count << " " << i << "\n";
    }
}

int main() {
    List<std::string> list;

    std::random_device rd;
    std::default_random_engine e{rd()};
    std::uniform_int_distribution<int> distChar{0, 25};
    std::uniform_int_distribution<int> distLen{2, 8};

    const int nodesToAdd = 100;

    int random_value = 0;
    size_t size = sizeof(random_value);
    std::ifstream urandom("/dev/urandom", std::ios::in|std::ios::binary); //Open stream
    if(urandom) //Check if stream is open
    {
        for (long long int i = 0;i< nodesToAdd; ++i){
            urandom.read(reinterpret_cast<char*>(&random_value), size); //Read from urandom
            if(urandom) //Check if stream is ok, read succeeded
            { std::string ir = std::to_string(random_value);
                list.append(ir);    //    std::cout << "Read random value: " << random_value << std::endl;

            }
            else //Read failed
            {
                std::cerr << "Failed to read from /dev/urandom" << std::endl;
            }
        }
        urandom.close(); //close stream
    }
    else //Open failed
    {
        std::cerr << "Failed to open /dev/urandom" << std::endl;
    }

    bool test1 = 1;
    bool test2 = 0;
    std::thread t1(task3, std::ref(list), std::ref(test1));
    std::thread t2(task3, std::ref(list), std::ref(test2));
    t1.join();
    t2.join();

    return 0;
}
