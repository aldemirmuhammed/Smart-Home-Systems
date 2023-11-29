#ifndef HOME_SERVER
#define HOME_SERVER

#include <stdio.h>          // printf(), scanf()
#include <stdlib.h>
#include <stdbool.h>        // true, false

#include <sys/socket.h>     // socket
#include <arpa/inet.h>      // socket
#include <netinet/in.h>     // socket
#include <netdb.h>          // socket
#include <errno.h>          // Error

#include <string.h>         // String

#include <signal.h>         // SIGINT
#include <pthread.h>        // pthread_t, pthread_create()
#include <semaphore.h>      // sem_init()

#include <math.h>           // pow()
#include <time.h>           // usleep()

#include <unistd.h>         // UART
#include <fcntl.h>          // UART
#include <termios.h>        // UART

#include "gpioDIO.h"        // GPIOWrite(), GPIORead()

#include "Configuration.h"  // Ayarlar; PASSWORD, MAX_CONNECTION ,NUMBER_OF_SENSOR vb...


//  TODO: Socket ve UART ayni okuma ve yazma fonksiyonlarini kullanabilir.
//  TODO: Signal handler içerisinde tüm clientlara mesaj yollanacak ve kapatılacak


/***  SENSORLERE AIT BILGILER  ***/

char *SENSOR_NAME[NUMBER_OF_SENSOR]    = {"Mesafe"};    // Sensor Tipleri
int   SENSOR_PINS[NUMBER_OF_SENSOR]    = {SENSOR_1};  // Sensor Pinleri
int   SENSOR_VALUES[NUMBER_OF_SENSOR];
/*********************************/

/***  SOCKET  ***/

int SERVER_SOCKET_FD;
int CONNECTED_DEVICE_FD[ MAX_CONNECTION ];
int CONNECTION_ORDER = 0;

struct sockaddr_in SERVER_ADDRESS;
struct sockaddr_in CONNECTED_DEVICE_STRUCT_LIST[MAX_CONNECTION];
/****************/

/******    UART   *******/

int UART_FILE_DESCRIPTOR = -1;
/************************/


/*** THREAD, SEMAPHORE, MUTEX ***/
sem_t semUpdateConnectedDeviceFD;
pthread_mutex_t mutex; //

pthread_t threadID_ReadUART, threadID_WriteUART;
pthread_t threadID_MakeConnection;
pthread_t checkSocketMessageThread;

pthread_t threadID_ReadSensorValue;
pthread_t threadID_AutoMode;
/*********************************/

/***    FONKSIYON PROTOTIPLERI  ***/

bool IsPasswordCorrect(char*);



int Respond(int, char *);

void DisplayInfoMessageAfterWriting(int, char *);
void CloseAllConnections(void);
void ManageComingRequest(char*, int*);
void MessageParse(char *, char *, int *);
void UpdateConnFDandConnDeviceStructList(int*);

void SignalHandlerShutdown(int);
void PrepareMessageForSensorList(char*);

/* *
 * UART Iletisimi icin gerekli
 * Degisken ve flagler ayarlanir.
 * */
void InitUART() {

    // Bloklanmayan modda yazma ve okuma
    UART_FILE_DESCRIPTOR = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);
    
    if (UART_FILE_DESCRIPTOR == -1)
        printf("Error - Unable to open UART. Ensure it is not in use by another application\n");
    
    struct termios options;
    tcgetattr(UART_FILE_DESCRIPTOR, &options);          // terminal ozelliklerini al
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;     // baudrate'i ayarla
    options.c_iflag = IGNPAR;                           // parity'i devre disi birak
    options.c_oflag = 0;                                // cikis modlari
    options.c_lflag = 0;                                // yerel modlar
    tcflush(UART_FILE_DESCRIPTOR, TCIFLUSH);            // Alinan fakat henuz okunmayan veriyi flushlar
    tcsetattr(UART_FILE_DESCRIPTOR, TCSANOW, &options); // Duzenlenmis terminal ozelliklerini ayarla
} // end InitUART()

/* *
 * Sokcet Programlama icin gerekli degiskenleri ayarlar
 * */
int  InitServerSocket(int serverPortNumber) {
    
    // Server Socket Adresi
    SERVER_ADDRESS.sin_family = AF_INET;
    SERVER_ADDRESS.sin_port = htons(serverPortNumber);
    SERVER_ADDRESS.sin_addr.s_addr = htonl(INADDR_ANY);
    
    //Yeni Socket Yaratiliyor
    SERVER_SOCKET_FD = socket(PF_INET, SOCK_STREAM, 0);
    if (SERVER_SOCKET_FD == -1) {
        printf("Error on socket creation [%s] \n", strerror(errno));
        return (-1);
    }
    
    // Adres server socketine baglaniyor
    int isBinded = bind(SERVER_SOCKET_FD, (struct sockaddr *) &SERVER_ADDRESS, sizeof(struct sockaddr));
    if (isBinded == -1) {
        printf("Error socket bind [%s] \n", strerror(errno));
        return (-1);
    }
    
    // Gelen baglanti istekleri dinleniyor
    if (listen(SERVER_SOCKET_FD, MAX_CONNECTION)) {
        printf("Error socket listen [%s] \n", strerror(errno));
        return (-1);
    }
    
    return 0;
} // end InitServerSocket(int serverPortNumber)



/* *
 * Gelen istekleri kabul eden thread - MakeConnection
 * */
void *threadMakeConnection(void *arg) {
    
    while (1)
    {
        if ( CONNECTION_ORDER < MAX_CONNECTION)
        {
            struct sockaddr_in clientStruct;
            
            //  Baglanti isteklerini kabul eder ve balnatı kurarak iletisim icin bir file descriptor dondurur.
            socklen_t addrSize = sizeof(clientStruct);
            CONNECTED_DEVICE_FD[ CONNECTION_ORDER ] = accept( SERVER_SOCKET_FD, (struct sockaddr *) &clientStruct, &addrSize );

            if (CONNECTED_DEVICE_FD[ CONNECTION_ORDER ] == -1) {
                printf("Error socket accept [%s] \n", strerror(errno));
                continue;
            }
            
            //  Socketi bloklanmasini onler.
            fcntl( CONNECTED_DEVICE_FD[CONNECTION_ORDER], F_SETFL, O_NONBLOCK);
            printf("Server:\t New connection has been established [%d / %s / %d]\n",
                   CONNECTED_DEVICE_FD[ CONNECTION_ORDER ],
                   inet_ntoa(clientStruct.sin_addr),
                   clientStruct.sin_port);
            
            CONNECTED_DEVICE_STRUCT_LIST[ CONNECTION_ORDER ] = clientStruct;
            CONNECTION_ORDER++;
        }// if ( CONNECTION_ORDER < MAX_CONNECTION)
    }// while
}//end makeConnection()

/* *
 * Sensörlerin durumu periyodik olarak (2 saniyede bir) okur, local değişkenlerde güncel değerleri tutulur.
 * */
void *threadReadSensorValues(void *arg) {
    
    while (1) {
        
        usleep((int) 2 * 1000000);                          // 2 saniye bekle
        
        for (int i = 0; i < NUMBER_OF_SENSOR ; i++) {
            
            int sensorValue = GPIORead( SENSOR_PINS[i] );   // Sensorler Okunuyor
            
            usleep((int) 1000000);                          // 1 saniye bekle
            
            if (sensorValue != -1) {
                SENSOR_VALUES[i] = sensorValue;
                printf("INFO - Pin Number [%d], Pin Value [%d]\n", SENSOR_PINS[i], SENSOR_VALUES[i]);
            }// if(sensorValue)
        } // for
    } // while(1)
} // end threadReadSensorValues()

/* *
 * Socketlerden gelen mesajları okur ve gerekli işlemlerin yapılabilmesi için ManageComingRequests fonksiyonunu çağırır.
 * */
void *threadCheckSOCKETMessage(void *arg) {

    int receivedMessageSize;
    char receivedMessage[100];
    
    while (1)
    {
        for (int indexConnectionOrder = 0; indexConnectionOrder < CONNECTION_ORDER; indexConnectionOrder++)
        {
            receivedMessageSize = read( CONNECTED_DEVICE_FD[indexConnectionOrder], receivedMessage, sizeof(receivedMessage));
            
            if (receivedMessageSize != -1) {                            //  Okuma işleminde herhangi bir hata yoksa
                
                receivedMessage[ receivedMessageSize ] = '\0';          //  Gelen mesajın sonuna null terminator ekle
                
                printf("From: [IP: %s] - [PORT: %d] \t Received Message: [%s]\n",   //  Mesaj bilgilerini yazdir.
                       inet_ntoa( CONNECTED_DEVICE_STRUCT_LIST[indexConnectionOrder].sin_addr ),
                       CONNECTED_DEVICE_STRUCT_LIST[indexConnectionOrder].sin_port,
                       receivedMessage);

                ManageComingRequest( receivedMessage, &CONNECTED_DEVICE_FD[indexConnectionOrder] ); // Uygun islemi gerceklestir.
                
            }// if receivedMessageSize != -1)
        }// for (int i = 0; i < CONNECTION_ORDER; i++)
    }// while
}// end threadCheckSOCKETMessage()

/* *
 * UART uzerinden mesaj yazma islemini gerceklestirir
 * */
void *threadWriteUART() {
    
    while (1)
    {
        char responseBuffer[20];
        
        if (UART_FILE_DESCRIPTOR != -1)
        {
            int count = write(UART_FILE_DESCRIPTOR, &responseBuffer[0], strlen(responseBuffer));
            if (count < 0)  printf("ERROR - Writing via UART\n");
        }// if
        sleep(1);
    }// while
    
}// threadWriteUART()

/* *
 * UART uzerinden gelen mesaj okunur.
 * */
void *threadReadUART() {
    
    while (1) {
        
        if (UART_FILE_DESCRIPTOR != -1) {
            
            char requestBuffer[256];
            
            int requestLength = read(UART_FILE_DESCRIPTOR, (void *) requestBuffer, 255);
            
            if (requestLength > 0) {                            // Baytlar alinir ise gerekli islem yap

                requestBuffer[requestLength] = '\0';
                printf("UART - [%i] bytes received, Message: [%s]\n", requestLength, requestBuffer);

                ManageComingRequest(requestBuffer, &UART_FILE_DESCRIPTOR); // Alinan mesaja uygun islem gerceklestiriliyor
            }
        }// if
        sleep(1);
    }// while
}// threadReadUART()

/* *
 * Otomatik moda gecildiginde kullanicinin ayarladigi sensor durumuna gore role acip kapar.
 * */
void* threadAutoMode(void* arg)
{
    char * message = (char *) arg;                      // ID ve Role Islemi Icin status bilgileri.

    int sensorId        = atoi( strtok( message, " ")); // Sensor ID aliniyor
    int sensorStatus    = atoi( strtok( message, " ")); // Role icin SensorDurum Bilgisi aliniyor.

    while( 1 )
    {
        ( SENSOR_VALUES[ sensorId ] == sensorStatus )?  // Role acma kapama islemi yapiliyor
        GPIOWrite(RELAY, 0):
        GPIOWrite(RELAY, 1);
    }
} // End threadAutoMode(void* arg)

/* *
 *  Main Fonksiyonu gerekli Thread, Semaphore, Mutex ayarlamalari
 * */
int main(int argc, char **argv) {
    
    //  PORT numarasini Komut satirindan arguman olarak almali.
    if (argc < 2) {
        printf("USAGE:\t [appname] [portNumber] \n");
        return -1;
    }

    // PORT numarasina gore socket ayarlaniyor.
    int serverPortNumber = atoi(argv[1]);
    if (InitServerSocket(serverPortNumber) != -1)
        printf("Server Socket is created up to %d connections.\n", MAX_CONNECTION);

    /* *
     * SIGINT Sinyali ( CTRL+C ),
     * Alindiginda Server'i kapatacak handler ayalamalari
     * */
    struct sigaction new_action, old_action;
    new_action.sa_handler=SignalHandlerShutdown;
    new_action.sa_flags=SA_NODEFER | SA_ONSTACK;
    sigaction(SIGINT, &new_action, &old_action);

    // Semaphore'u baslat
    sem_init(&semUpdateConnectedDeviceFD,0,0);

    // Mutex'i baslat
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex,0);
    
   

    //  Sensor değerlerini periyodik (2sn) olarak okuyacak thread
    pthread_create(&threadID_ReadSensorValue, NULL, threadReadSensorValues, NULL);
    
    // UART iletisimi icin ayarlamalar yapılıyor.
    InitUART();

    //  UART'dan yazma ve okuma islemlerini gerceklestirecek thread
    pthread_create(&threadID_ReadUART, NULL, threadReadUART, NULL);
    sleep(1);
    pthread_create(&threadID_WriteUART, NULL, threadWriteUART, NULL);

    //  Socket baglantisi uzerinden gelen istekleri kabul eden thread
    pthread_create(&threadID_MakeConnection, NULL, threadMakeConnection, NULL);
    
    // Socket baglantisi uzerinden gelen mesajlari yoneten thread
    pthread_create(&checkSocketMessageThread, NULL, threadCheckSOCKETMessage, NULL);

    

    // Tum baglantılar kapatiliyor
    CloseAllConnections();

    pthread_mutex_destroy(&mutex);
    sem_destroy(&semUpdateConnectedDeviceFD);

    return 0;
} // end Main()

bool IsPasswordCorrect(char* password) {

    return (!strcmp(password,PASSWORD)) ? true : false;
}

/***********************************************/

/* *
 * UART veya Socket ile gelen istek mesajini komut ve argumanlarina(varsa) ayristirir.
 * */
void MessageParse(char *message, char *command, int *argument) {

    char *sensorToken;
    sensorToken = strtok(message, ":");

    if(! strncmp(sensorToken,"autoMode",8))
        strcpy(command,sensorToken);
    else
        sscanf(sensorToken, "%s", command);

    if (!strcmp(command, "sensorTip") ||
        !strcmp(command, "surucu"))
        sscanf(message, "%s %d", command, argument);

} // End MessageParse

/* *
 * UART veya Socket uzerinden gelen istek mesajlarini yoneten fonksiyon
 * Fonksiyonun tamami bir kritik bolgedir. Dolayisiyla mutex kullanilir.
 * */
void ManageComingRequest(char* request, int* connDevice_fd){
    
    // Kritik bölgeye giriliyor
    pthread_mutex_lock(&mutex);
    
    char command[20];   // Gelen komut
    int argument = -1;  // Gelen arguman

    // Gelen Mesaj Komut ve Argumanlarina(varsa) ayristirilir.
    MessageParse(request, command, &argument);

    printf("INFO - Request  has Command: [%s], Argument [%d]\n", command, argument);

    char responseBuffer[50];
    if (!strcmp(command, "sensorDurum")) {                          // Gelen komut sensorDurum ise
        int value = 0;
        for (int i = 0; i < NUMBER_OF_SENSOR; i++)                  // <durum> bilgisi icin bit degerleri ayarlaniyor.
            value += SENSOR_VALUES[i] * pow(2, i);

        sprintf(responseBuffer, "sensorDurum %d:", value);
        DisplayInfoMessageAfterWriting(*connDevice_fd, responseBuffer);

    }// if(sensorDurum)

    else if (!strcmp(command, "sensorSayi")) {                              // Gelen komut "sensorSayi" ise

        sprintf(responseBuffer, "sensorSayi %d:", NUMBER_OF_SENSOR);

        DisplayInfoMessageAfterWriting(*connDevice_fd, responseBuffer);
        
    }//else if (sensorSayi)
    
    else if (!strcmp(command, "sensorTip")) {                               // Gelen komut "sensorTip" ise

        sprintf(responseBuffer, "sensorTip %s:", SENSOR_NAME[argument]);
        DisplayInfoMessageAfterWriting(*connDevice_fd, responseBuffer);
        
    }//else if (sensorTip)
    
    else if (!strcmp(command, "surucu")) {                                  // Gelen komut "surucu" ise
        
        int ret = GPIOWrite(RELAY, argument);
        
        sprintf(responseBuffer, "surucu %s:", ret < 0 ? "Error" : "OK");
        DisplayInfoMessageAfterWriting(*connDevice_fd, responseBuffer);

    }//else if (surucu)
    
    else if (!strcmp(command, "surucuDurum")) {                             // Gelen komut "surucuDurum" ise

        sprintf(responseBuffer, "surucuDurum %d:", GPIORead(RELAY));
        DisplayInfoMessageAfterWriting(*connDevice_fd, responseBuffer);
        
    }//else if (surucuDurum)

    else if (!strncmp(command, "autoMode", 8)) {                            // Gelen komut "autoMode" ise

        char *duplicatedMessageWithoutEndDelimeter, *message;

        duplicatedMessageWithoutEndDelimeter = strdup( command );

        message = strtok( duplicatedMessageWithoutEndDelimeter, " ");       // autoMode çıkarıldı.Orn; [on 1 1] kaldı.

        while( message != NULL )
        {
            message = strtok(NULL," ");                                     //  on | off kısmı çıkarıldı.
            if ( !strcmp( message, "on"))
            {
                printf("INFO - Automode ON!\n");
                pthread_create( &threadID_AutoMode, NULL, threadAutoMode, (void*) message);   // id ve status olan kısım verildi.
                break;
            }
            else
            {
                printf("INFO - Automode OFF!\n");
                pthread_cancel( threadID_AutoMode );

                sprintf(responseBuffer, "autoMode %s:", "OK");
                DisplayInfoMessageAfterWriting(*connDevice_fd,responseBuffer);

                break;
            }
        }// end while

    }//else if (autoMode)

    else if (!strcmp(command, "sensorList")) {                              // Gelen komut "sensorList" ise

        char responseListbuffer[256];

        responseListbuffer[0]='\0';

        PrepareMessageForSensorList(responseListbuffer);                    // SensorListesi icin mesaj hazirlaniyor.

        DisplayInfoMessageAfterWriting(*connDevice_fd,responseListbuffer);

    }//else if (sensorList)

    else if (!strcmp(command, "close")) {                                   // Gelen komut "close" ise

        UpdateConnFDandConnDeviceStructList(connDevice_fd);                 // Ilgili baglantiyi kapat
        sem_wait(&semUpdateConnectedDeviceFD);                              // CONNECTED_DEVICE_FD guncellenmesi icin
                                                                            // ilgili baglanti kapanana kadar bekle
        
    }//else if (surucuDurum)

    // Kritik bolgeden cikiliyor...
    pthread_mutex_unlock(&mutex);
} // end ManageComingRequest

/* *
 * UART veya Socket ile cevap mesajini ilgili file descriptor'a yazar.
 * */
int Respond(int connectedDeviceFD, char *response) {
    return write(connectedDeviceFD, response, strlen(response)) == -1 ? false : true;
} // end Respond()

/* *
 *  Respond() fonksiyonunu kullanarak ilgili mesaji yazmayi dener ve durum bilgisini ekrana bastirir.
 * */
void DisplayInfoMessageAfterWriting(int connectedDeviceFD, char *response) {
    (!Respond(connectedDeviceFD, response)) ?
    printf("ERROR: Writing: [%s], FD: [%d]\n", response, connectedDeviceFD):
    printf("OK:    Writing: [%s], FD: [%d]\n", response, connectedDeviceFD);
} //end DisplayInfoMessageAfterWriting()

/* *
 *  Sensor Listesini hazirlar
 * */
void PrepareMessageForSensorList(char* sensorListMessage) {

    for (int id = 0; id < NUMBER_OF_SENSOR ; id++) {

        strcat(sensorListMessage, SENSOR_NAME[id]);

        if (id == NUMBER_OF_SENSOR - 1)
            break;

        strcat(sensorListMessage, ",");
    }
    strcat(sensorListMessage, ":");

    printf("\nINFO - Prepared SensorList: [%s]\n",sensorListMessage);
} // end PrepareMessageForSensorList

/* *
 *  Client tarafindan kapatilma istegi geldiginde
 *  CONNECTED_DEVICE_FD,CONNECTED_DEVICE_STRUCT_LIST listelerini gunceller.
 * */
void UpdateConnFDandConnDeviceStructList(int* connDevice_fd){

    for(int indexOfConnectionFd = 0 ; indexOfConnectionFd < CONNECTION_ORDER; indexOfConnectionFd++) {

        if (CONNECTED_DEVICE_FD[indexOfConnectionFd] == *connDevice_fd) {

            CONNECTION_ORDER--;
            for (int updateIndexes = indexOfConnectionFd; updateIndexes < CONNECTION_ORDER; updateIndexes++) {

                CONNECTED_DEVICE_FD[updateIndexes] = CONNECTED_DEVICE_FD[updateIndexes + 1];
                CONNECTED_DEVICE_STRUCT_LIST[updateIndexes] = CONNECTED_DEVICE_STRUCT_LIST[updateIndexes + 1];
            } // for

            break;
        } // if
    } // for
    if(close(*connDevice_fd) < 0)
        printf("\nERROR - Cannot Closed FD[%d]\n", *connDevice_fd);

    sem_post(&semUpdateConnectedDeviceFD);                              // Semphore'u serbest birakir.
} // end UpdateConnFDandConnDeviceStructList()

/* *
 *  SIGINT(CTRL + C) sinyali alindiginda ilk once var olan tum threadleri kapatir,
 *  daha sonra varolan tum baglantilar sonlandirilir.
 * */
void SignalHandlerShutdown(int signalNo)
{
    if( signalNo == SIGINT ){

        printf("\nINFO - Server is shutting down...\n");

        pthread_cancel(threadID_MakeConnection);
        pthread_cancel(threadID_ReadUART);
        pthread_cancel(threadID_WriteUART);

        pthread_cancel(threadID_ReadSensorValue);

        CloseAllConnections();
    }
} // end SignalHandlerShutdown()

/* *
 *  Varolan tum file descriptorlari kapatarak baglantilari sonlandirir.
 * */
void CloseAllConnections() {

    for (int i = 0; i < CONNECTION_ORDER; i++)
        close(CONNECTED_DEVICE_FD[i]);

    close(SERVER_SOCKET_FD);
    close(UART_FILE_DESCRIPTOR);
} // end CloseAllConnections

#endif
