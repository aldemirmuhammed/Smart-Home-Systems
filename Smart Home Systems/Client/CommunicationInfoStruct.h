#ifndef COMMUNICATIONINFO_H
#define COMMUNICATIONINFO_H

typedef struct {

    int communication_fd;
    int port;
    char ip[20];

}CommunicationInfo;

#endif
