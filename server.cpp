// Server side implementation of UDP echo server
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 50001
#define MAX_PLAYER_NUM 2

enum PacketType {
    PositionUpdate
};

struct Vec3 {
    double x;
    double y;
    double z;
};

struct PositionPacket {
	PacketType packetType;
	uint32_t num;
	char playerName[32];
	Vec3 position;
};

struct Player {
	sockaddr_in addr;
	PositionPacket posPacket;
	std::chrono::time_point<std::chrono::steady_clock> lastPacketSentTime;
};

std::unordered_map<std::string, Player> clients;
std::mutex m;

void timeOutPlayers() {
	while(true) {
		auto timeNow = std::chrono::steady_clock::now();
		m.lock();
		for (auto &player : clients) {
			auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(timeNow - player.second.lastPacketSentTime);
			if (elapsedTime >= std::chrono::seconds(10)) {
				char ipAddressChar[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(player.second.addr.sin_addr), ipAddressChar, INET_ADDRSTRLEN);
				std::string targetIp(ipAddressChar);
				std::cout << "Client with IP " << targetIp << " timed out" << std::endl;
				clients.erase(targetIp);
				break; // Break because it will otherwise loop over the rest of a not changed hashmap
			}
		}
		m.unlock();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

int main() {
    int sockfd;
    sockaddr_in serveraddr;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serveraddr, 0, sizeof(serveraddr));

    // Filling server information
    serveraddr.sin_family = AF_INET; // IPv4
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(PORT);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
	
	char buffer[sizeof(PositionPacket)] = {};
	std::thread t1(timeOutPlayers);
	t1.detach();
	
	while(true) {
		sockaddr_in clientAddr;
		memset(&clientAddr, 0, sizeof(clientAddr));
        socklen_t clientAddrLen = sizeof(clientAddr);

		// Receive messages
		int messageLen;
		messageLen = recvfrom(sockfd, (char*)buffer, sizeof(PositionPacket), MSG_WAITALL, (struct sockaddr*)&clientAddr, &clientAddrLen);
		
		// Check if client is already in client list
		char ipAddressChar[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(clientAddr.sin_addr), ipAddressChar, INET_ADDRSTRLEN);
		std::string ipAddress(ipAddressChar);
		
		//std::cout << "Got packet from " << ipAddress << std::endl;
		
		// Check if client is a new one
		m.lock();
		if(clients.count(ipAddress) == 0) {
			if(clients.size() >= MAX_PLAYER_NUM) {
				// Send back empty packet as sign that the lobby is full
				char lobbyFull[sizeof(PositionPacket)] = {};
				sendto(sockfd, (char*)lobbyFull, sizeof(PositionPacket), MSG_CONFIRM, (struct sockaddr*)&clientAddr, sizeof(sockaddr_in));
				continue;
			} else {
				// Lobby is not full. Add player to client list
				Player player;
				player.addr = clientAddr;
				memcpy(&player.posPacket, &buffer, sizeof(PositionPacket));
				player.lastPacketSentTime = std::chrono::steady_clock::now();
				clients[ipAddress] = player;
				std::cout << "Client with IP " << ipAddress << " connected" << std::endl;
			}
		} else {
			// Check if port doesnt match
			if(clientAddr.sin_port != clients[ipAddress].addr.sin_port) {
				// Probably a reconnect
				clients[ipAddress].addr.sin_port = clientAddr.sin_port;
				std::cout << "Client with IP " << ipAddress << " reconnected" << std::endl;
			}
			
			memcpy(&clients[ipAddress].posPacket, &buffer, sizeof(PositionPacket));
			clients[ipAddress].lastPacketSentTime = std::chrono::steady_clock::now();
		}
		m.unlock();
		
		m.lock();
		for (auto &player : clients) {
			if(clientAddr.sin_addr.s_addr != player.second.addr.sin_addr.s_addr) { // For testing purposes it sends the packet back
				char sendBuffer[sizeof(PositionPacket)] = {};
				memcpy(&sendBuffer, &player.second.posPacket, sizeof(PositionPacket));
				sendto(sockfd, (char*)sendBuffer, sizeof(PositionPacket), MSG_CONFIRM, (struct sockaddr*)&clientAddr, sizeof(sockaddr_in));
			}
		}
		m.unlock();
	}

    return 0;
}
