//12.07.2024	Sindre Taraldlien	"Dynon Coding Project" 

//#############################
//###       Includes        ###
//#############################


#include <iostream>
#include <mutex>
#include <thread>
#include <vector> 
#include <string>			//-----> All not required
#include <cstring>
#include <cstdlib>
#include <mqueue.h>
#include <condition_variable>
#include <chrono>



//#############################
//###   Global arguments    ###
//#############################


int debugging = 0;

// The structure as specified by the project assignment
struct givenStruct {
	int iVal;
	float fVal;
	std::string sVal;
	enum Type { type1, type2, type3 } type;
	};

// Using switch cases to determine the type then returning it in a string format
const char* convertTp2Str(givenStruct::Type type) {
	switch (type) {
		case givenStruct::type1: return "type1";
		case givenStruct::type2: return "type2";
		case givenStruct::type3: return "type3";
		default: return "unknown type!";
		}
	}	



// A mutex used to synchronize the two operating threads ensuring predictability
std::mutex mediator;

// Signals for the messenger and receiver to use for synchronization
std::condition_variable messengerSignal;
std::condition_variable receiverSignal;



//#############################
//###  Messenger function   ###
//#############################


// Function for the messenger part of the software, threaded in main
void messenger() {



//###  Messenger setup  ###


	//If the queue already exists this deletes it
	if (mq_unlink("/myVals") == -1 && errno != ENOENT) {
		std::cerr << "Error unlinking message queue: " << strerror(errno) << std::endl;
		return;
		}

	//Setting attributes for the message queue
	struct mq_attr attr;
	attr.mq_flags = 0;
	attr.mq_maxmsg = 10;
	attr.mq_msgsize = sizeof(givenStruct);
	attr.mq_curmsgs = 0;
	
	//Creating the message queue and opening for writing
	mqd_t messageQ = mq_open("/randomVals", O_CREAT | O_WRONLY | O_NONBLOCK, 0666, &attr);
	
	//Error message in case the message queue could not be created
	if (messageQ == (mqd_t)-1) {
		std::cerr << "Could not create/open message queue: " << strerror(errno) << " ... exiting..." << std::endl;
    		return;
		}
	
	struct mq_attr current_attr;
 
 	//Error message in case the message queue could not be opened
	if (messageQ == (mqd_t)-1) {
		std::cerr << "Could not open the message queue... exiting..." << std::endl;
		return;
		}



//### Messenger loop  ###


	//For loop that sends the structure through the message queue
	for (int i = 1; i < 11; i++) {
	
		givenStruct content;
		content.iVal = i * 1;
		content.fVal = i * 1.1f;
		content.sVal = "String" + std::to_string(i);
		content.type = static_cast<givenStruct::Type>(i % 3);

		std::unique_lock<std::mutex> lock(mediator);

		//Print statement to show we have reached the loop before sending the message
		if (debugging == 1){
		
			std::cout << "Messenger sending message..." << std::endl;
			
			}

		//Sending the message to the message queue
		if (mq_send(messageQ, reinterpret_cast<const char*>(&content), sizeof(content), 0) == -1) {
		
			std::cerr << "Could not send the message... exiting..." << std::endl;
			return;
			
			}
		
		//Print statement to show we have reached the loop after sending the message
		if (debugging == 1){
		
			std::cout << "Message sent..." << std::endl;
			
			}
		
		//Debugging used for troubleshooting, can be applied by running ./*program name* d
		if (debugging == 1){
	
        		if (mq_getattr(messageQ, &current_attr) == -1) {
        			std::cerr << "Could not get queue attributes: " << strerror(errno) << std::endl;
        		} else {
        			std::cout << std::endl << "----||----" << std::endl << "Queue size: " << 
        			current_attr.mq_curmsgs << " messages." << std::endl << "----||----" << std::endl << std:: endl;
        			}
        			
        		}

		//Locks to ensure synchronization between threads, notify sends a signal to the 
		//other thread and removes the lock, wait makes this thread wait for a signal
		receiverSignal.notify_one();
		messengerSignal.wait(lock);
		
		}

	//Closing the message after use is done
	if (mq_close(messageQ) == -1) {
	
		std::cerr << "Could not close the message queue... exiting..." << std::endl;
		
		}
		
	}



//#############################
//###   Receiver function   ###
//#############################


// Function for the receiver part of the software, threaded in main
void receiver() {



//### Receiver setup  ###


	//Locking this thread and waiting for signal, this thread cannot miss the first signal
	std::unique_lock<std::mutex> lock(mediator);
	receiverSignal.wait(lock);
	
	//Opening the message queue for reading only
	mqd_t messageQ = mq_open("/randomVals", O_RDONLY);

	//Error message in case the queue cannot be opened
	if (messageQ == (mqd_t)-1) {
	
		std::cerr << "Could not open the message queue... exiting..." << std::endl;
		return;
		
	}
	
	givenStruct content;



//### Receiver loop  ###


	//For loop dealing with the reading of the message queue
	for (int i = 1; i < 11; i++) {
	
		if (debugging == 1){
		
			std::cout << "Receiver reading message..." << std::endl;
			
			}
	
		//Receiving the message
		ssize_t bytesRead = mq_receive(messageQ, reinterpret_cast<char*>(&content), sizeof(content), nullptr);

		//Error in case the message was not received
		if (bytesRead == -1) {
			std::cerr << "Did not receive message... exiting..." << std::endl;
			return;
		}

		if (debugging == 1){
		
			std::cout << "Message read by receiver..." << std::endl;
			
			}

		//Spacing for better reading in "debug" mode
		if (debugging == 1){
		
			std::cout << std::endl;
			
			}

		//Printing the struct
		std::cout << "int: " << content.iVal
			  << ", float: " << content.fVal
			  << ", string: " << content.sVal
			  << ", enum: " << convertTp2Str(content.type) << std::endl;
			  
		//Spacing for better reading in "debug" mode
		if (debugging == 1){
		
			std::cout << std::endl;
			
			}

		//Sending back a signal telling the messenger the messenge was received
		messengerSignal.notify_one();
		
		//Checks whether it is the final iteration (to prevent a non-termination of the program)
		if (i == 10){
		
			exit(1);
		
			}
		
		//Waiting for the messenger's signal
		receiverSignal.wait(lock);
		
	}

	//Closing the queue
	if (mq_close(messageQ) == -1) {
		std::cerr << "Could not close the message queue... exiting..." << std::endl;
		}

	//Removing the queue
	if (mq_unlink("/randomVals") == -1) {
		std::cerr << "Could not delete the message queue... exiting..." << std::endl;
		}
		
	exit(1);
		
	}



//#############################
//###    Main function      ###
//#############################


int main(int argc, char **argv) {

	if (std::stoi(argv[1]) == 2){
	
		debugging = 1;
		
	
		}

	// Creating both threads
	std::thread receiverThread(receiver);
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	std::thread messengerThread(messenger);
	
	// Joining threads to wait until they've completed execution before ending the software/main
	messengerThread.join();
	receiverThread.join();

	return 0;
}

