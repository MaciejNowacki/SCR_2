#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/neutrino.h>
#include <process.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/netmgr.h>

#define SIZE 8

int main(int argc, char *argv[]) {
	pid_t pId;
	int channelId;
	srand(time(NULL));

	if((channelId = ChannelCreate(0)) == -1) { //proces potomny odziedziczy channelId
		perror("Oops: channel create error");
		return EXIT_FAILURE;
	}

	if((pId = fork()) == -1) {
		perror("Oops: fork error");
		return EXIT_FAILURE;
	}

	if(pId == 0) { //część kliencka
		int connectionId;
		int shmId;
		char rmsg[32];
		char nameOfShm[32] = "nameOfShm";
		int *pMemory; //wskaźnik na adres zaalokowanej pamięći współdzielonej

		if((connectionId = ConnectAttach(ND_LOCAL_NODE, getppid(), channelId, 0, 0)) == -1) { //getppid() - pobiera PID procesu macierzystego, ND_LOCAL_NODE - lokalna maszyna
			perror("Oops: connection attach error");
			return EXIT_FAILURE;
		}

		if((shmId = shm_open(nameOfShm, O_RDWR| O_CREAT, 00777)) == -1) { //O_RDWR - czytanie i pisanie, O_CREAT - jeżeli nie istnieje to utwórz
			perror("Oops: share memory open error");
			return EXIT_FAILURE;
		}

		if(ftruncate(shmId, SIZE * sizeof(int)) == -1) {
			perror("Oops: truncate a file error");
			return EXIT_FAILURE;
		}

		if((pMemory = (int*) mmap(NULL, SIZE * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shmId, 0)) == MAP_FAILED) { //NULL = 0?
			perror("Oops: nmap error");
        	return EXIT_FAILURE;
		}

		generateData(pMemory);
		printData(pMemory);
		printf("[CLIENT] Sent request...\n");

		if(MsgSend(connectionId, &nameOfShm, sizeof(nameOfShm), &rmsg, sizeof(rmsg)) == -1) {//&msg czy bez &
			perror("Oops: MsgSend error");
			return EXIT_FAILURE;
		}

		printData(pMemory);
		munmap(pMemory, SIZE * sizeof(int));
		shm_unlink(nameOfShm);
    	close(shmId);
		ConnectDetach(connectionId);
		return EXIT_SUCCESS;
	} else { //część serwerowa
		int receiveId;
		char nameOfShm[32];
		int shmId;
		int *pMemory; //wskaźnik na adres zaalokowanej pamięći współdzielonej
		int pStatus; //status procesu nadrzędnego, do użytku na koniec

		if((receiveId = MsgReceive(channelId, &nameOfShm, sizeof(nameOfShm), NULL)) == -1) { //NULL - możliwy wskaźnik na jakąś dodatkową strukturę, która zawiera X dane
			perror("Oops: MsgReceive error");
			return EXIT_FAILURE;
		}

		if((shmId = shm_open(nameOfShm, O_RDWR| O_CREAT, 00777)) == -1) { //O_RDWR - czytanie i pisanie, O_CREAT - jeżeli nie istnieje to utwórz
			perror("Oops: share memory open error");
			return EXIT_FAILURE;
		}
		
		if((pMemory = (int*) mmap(NULL, SIZE * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shmId, 0)) == MAP_FAILED) { //NULL = 0?
			perror("Oops: nmap error");
        	return EXIT_FAILURE;
		}

		printf("[SERVER] Sorting...\n");
		sortNumbers(pMemory);

		munmap(shmId, SIZE * sizeof(int));
	    close(shmId);
	    shm_unlink(nameOfShm);

		if(MsgReply(receiveId, 0, &nameOfShm, sizeof(nameOfShm)) == -1) { //0 - status?
			perror("Oops: MsgReply error");
        	return EXIT_FAILURE;
		}

		do{
			if(waitpid(pId, &pStatus, WNOHANG) == -1) {
				perror("Oops: waitpid error");
				return EXIT_FAILURE;
			}
		}while(WIFEXITED(pStatus) == 0);
	}
	ChannelDestroy(channelId);
	return EXIT_SUCCESS;
}

void generateData(int *pMemory) {
    for(int i = 0; i < SIZE; i++) {
		pMemory[i] = rand()%999;
	}
}

void printData(int *pMemory) {
	printf("------------------------------------------\n");
	for(int i = 0; i < SIZE; i++) {
		printf("%d ", pMemory[i]);
	}
	printf("\n------------------------------------------\n");
}

void sortNumbers(int *pMemory) {
	int tmp;

	for(int i = 0; i < SIZE-1; i++) {
		for(int j = 0; j < SIZE-i-1; j++) {
			if(pMemory[j] > pMemory[j+1]) {
				tmp = pMemory[j];
				pMemory[j] = pMemory[j+1];
				pMemory[j+1] = tmp;
			}
		}
	}
}
