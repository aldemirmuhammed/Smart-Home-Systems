#ifndef COMMUNICATION_H
#define COMMUNICATION_H

// Bool için
#include <stdbool.h>

// Socket Programlama içim
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

// UART için
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

// Hata için
#include <errno.h>

// String için
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include "Configuration.h"                  // NUMBER_OF_SENSOR
#include "CommunicationInfoStruct.h"        // CommunicationInfo -> Struct
#include "MessageHolderStruct.h"            // MessageHolder     -> Struct


bool ConnectWithUART   ( CommunicationInfo* );
bool ConnectWithSocket ( CommunicationInfo* );
bool CloseConnection   ( CommunicationInfo* );

bool RequestPackage( CommunicationInfo*, MessageHolder );
void ReceivePackage( CommunicationInfo*, MessageHolder* );

void ParseResponse ( char*, MessageHolder* );
void PrepareRequest( char*, int, MessageHolder* );

void DisplayResponseMessage ( MessageHolder );
void DisplaySensorList      ( CommunicationInfo* );

#endif
