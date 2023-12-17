#include <assert.h>
#include <stdint.h>
#include <stdio.h>


#include "ip.h"
#include "config.h"
#include "util.h"
#include "log.h"


// bascially I need to add some sort of global structure as my forwarding table
/*
 * Add a forwarding entry to your forwarding table.  The network address is
 * given by the network and netlength arguments.  Datagrams destined for
 * this network are to be sent out on the indicated interface.
 */

typedef struct TrieNode {
    int interface; // Interface to use, -1 if not a valid endpoint
    struct TrieNode *children[2]; // Children for 0 and 1 bit values
} TrieNode;

int aliveInterfaces[8] = {0, 0, 0, 0, 0, 0, 0, 0};
TrieNode *forwardingTableRoot = NULL;

TrieNode* createNode() {
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    node->interface = -1; // -1 indicates this is not an endpoint
    node->children[0] = node->children[1] = NULL;
    return node;
}

void insert(uint32_t network, int netlength, int interface) {
    TrieNode *current = forwardingTableRoot;
    for (int i = 31; i >= 0; i--) {
        int bit = (network >> i) & 1;
        if (current->children[bit] == NULL) {
            current->children[bit] = createNode();
        }
        current = current->children[bit];

        // Update the interface at this node if the current depth is within the netlength
        if (31 - i < netlength) {
            current->interface = interface;
        }
    }
}
// 
int lookup(uint32_t ip) {
    TrieNode *current = forwardingTableRoot;
    int lastInterface = -1;
    for (int i = 31; i >= 0; i--) {
        int bit = (ip >> i) & 1;
        // printf("Checking bit %d: %d\n", i, bit); // Debugging: Print the current bit being checked
        if (current->children[bit] == NULL) {
            // printf("No child at bit %d, breaking out\n", i); // Debugging: Print when breaking out
            break;
        }
        current = current->children[bit];
        // printf("Current interface at level %d: %d\n", 31 - i, current->interface); // Debugging: Print the interface at the current node
        if (current->interface != -1) {
            lastInterface = current->interface;
            // printf("Updating lastInterface to %d\n", lastInterface); // Debugging: Print when updating lastInterface
        }
    }
    return lastInterface; // Returns -1 if no matching entry is found
}


uint32_t generateSubnetMask(int hostBits) {
    // Ensure hostBits is within valid range (0-32 for IPv4)
    if (hostBits < 0 || hostBits > 32) {
        fprintf(stderr, "Invalid number of host bits\n");
        return 0;
    }
    // Calculate the subnet mask
    uint32_t subnetMask = 0xFFFFFFFFU << (32 - hostBits);

    return subnetMask;
}


// 142.103.10.0/24
// addForwardEntry(0x8e670a00, 24, 3);
// 0x8e is 142, 0x67 is 103, 0x0a is 10, and 0x00 is 0
void addForwardEntry(uint32_t network, int netlength, int interface) {

    // if (aliveInterfaces[interface] != 1) {
    //     printf("COULDN'T ADD FORWARD ENTRY BC INTERFACE DNE\n");
    //     return;
    // }
        printf("network %d.%d.%d.%d \n",
                (network & 0xFF000000) >> 24,
                (network  & 0x00FF0000) >> 16,
                (network  & 0x0000FF00) >> 8,
                (network  & 0x000000FF));
        printf("netlength %d, interface %d \n", netlength, interface);
    insert(network, netlength, interface);
}


/*
 * Add an interface to your router. 
 */
void addInterface(int interface) {
    aliveInterfaces[interface] = 1;
    // for(int i = 0; i < 8; i++)
    //   printf("%d \n", aliveInterfaces[i]);
    }



void handleSend(int fd, int interface, packet *pkt) {
}

// if outgoing link is same direction, just drop it 
// similarly, if the incoming packet doesnt matchw ith anything just drop it
// decrement TTL by 1, if it reaches 0 then jsut drop it 
/*
time to live reached 1 or 0
incorrect checksum
port not available / doesn't exist
incoming port is same as outgoing port
*/
void handleIncomingPackets(int fd) {
    while (1) {
        int interface;
        packet packet;
        int bytesRead = readpkt(fd, &packet, &interface);
        // ntohHdr((ipheader *)&packet);
        ipheader *sipHeader = (ipheader *) &packet;
        unsigned int destinationAddress = sipHeader->dstipaddr;

        // printf("PACKET DESTINATION %d.%d.%d.%d \n",
        //         (destinationAddress & 0xFF000000) >> 24,
        //         (destinationAddress  & 0x00FF0000) >> 16,
        //         (destinationAddress  & 0x0000FF00) >> 8,
        //         (destinationAddress  & 0x000000FF));

        if (bytesRead <= 0) {
            // Handle read failure or no bytes read
            continue;
        } else {
            htonHdr((ipheader *)&packet);
            int outInterface = lookup(destinationAddress);
            // if (outInterface != interface) {
                printf("PACKET %d.%d.%d.%d GOING TO INTERFACE %d\n",
                (destinationAddress & 0xFF000000) >> 24,
                (destinationAddress  & 0x00FF0000) >> 16,
                (destinationAddress  & 0x0000FF00) >> 8,
                (destinationAddress  & 0x000000FF), outInterface);
                sendpkt(fd, outInterface, &packet);
            // }
            // sendpkt(fd, interface, &packet);
        }
    }
}


void printTrieHelper(TrieNode *node, int level, uint32_t prefix) {

    if (node == NULL) {
        return;
    }

    // If this is a valid endpoint, print the interface
    if (node->interface != -1) {
        printf("Level %d - Prefix %d.%d.%d.%d/%d, Interface: %d\n",
               level,
               (prefix >> 24) & 0xFF, (prefix >> 16) & 0xFF, 
               (prefix >> 8) & 0xFF, prefix & 0xFF, 
               level, node->interface);
    }

    // Recursively print children
    if (node->children[0] != NULL) {
        printTrieHelper(node->children[0], level + 1, prefix);
    }
    if (node->children[1] != NULL) {
        printTrieHelper(node->children[1], level + 1, prefix | (1 << (31 - level)));
    }
}

void printTrie() {
    printf("Printing Trie:\n");
    printTrieHelper(forwardingTableRoot, 0, 0);
}

void run() {
    int port = getDefaultPort();
    int fd = udp_open(port);
    printTrie();
    if (fd < 0) {
        fprintf(stderr, "Error opening UDP socket\n");
        exit(EXIT_FAILURE);
    
    }

    handleIncomingPackets(fd);
    // Optionally, you may need to close the socket when done
}

int main(int argc, char **argv) {
    logConfig("router", "packet,error,failure");
    forwardingTableRoot = createNode();
    char *configFileName = "router.config";
    if (argc > 1) {
	configFileName = argv[1];
    }
    configLoad(configFileName, addForwardEntry, addInterface);
    run();
    return 0;
}
