#include "CommunicationAPI.h"

/* *
 * UART Baglantisi gerekli ayarlamalari yapar.
 * */
bool ConnectWithUART( CommunicationInfo* comInfo){

    // Blocklanmayan modda yazma ve okuma islemleri icin dosyayi actik.
    comInfo->communication_fd = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY);

    if (comInfo->communication_fd == -1) 
       return false;
       

    // Termios struct yapisini ayarlıyoruz.
    struct termios options;
    tcgetattr(comInfo->communication_fd, &options); // teminal ozelliklerini verir
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD; // baudrate 9600 ayarlandi
    options.c_iflag = IGNPAR;                       // parity bit etkinsizleştirildi
    options.c_oflag = 0;                            // cıkıs modlari
    options.c_lflag = 0;                            // yerel modlar
    tcflush(comInfo->communication_fd, TCIFLUSH);   // alinip okunmayan verileri flushla
    tcsetattr(comInfo->communication_fd, TCSANOW, &options); // degistirilmis terminal ozelliklerini degistir.

    return true;
}

/* *
 * Socket Baglantisi gerekli ayarlamalari yapar.
 * */
bool ConnectWithSocket( CommunicationInfo* commInfo)
{

    struct sockaddr_in clientStruct;

    // Socket Adresi
    clientStruct.sin_family = AF_INET;
    clientStruct.sin_port = htons(commInfo->port);

    if (inet_aton(commInfo->ip, &clientStruct.sin_addr) == 0)
        return false;

    // Socket yaratiliyor
    commInfo->communication_fd = socket(PF_INET, SOCK_STREAM, 0);

    // Baglanma problemlerini cozen bir setsockopt adinda fonksiyon SO_REUSEADDR flag'i ile kullaniliyor.
    if (setsockopt(commInfo->communication_fd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0){
        printf("Error on setsockopt creation [%s] \n", strerror(errno));
        return false;
    }

    if (commInfo->communication_fd == -1) {
        printf("Error on socket creation [%s] \n", strerror(errno));
        return false;
    }

    // Baglanti saglaniyor.
    if (connect(commInfo->communication_fd, (const struct sockaddr *) &clientStruct, sizeof(struct sockaddr)) == -1) {
        printf("Error on socket connect [%s] \n", strerror(errno));
        return false;
    }

    return true;
} // end ConnectWithSocket

/* *
 * Verilen ilgili file descriptor'ı kapatir.
 * */
bool CloseConnection( CommunicationInfo* commInfo ){

    int status = close(commInfo->communication_fd);

    return ( status < 0 ) ? false : true;
} // end CloseConnection()

/* *
 * UART veya Socket aracaligiyla mesaj verisi gonderiliyor
 * */
bool RequestPackage( CommunicationInfo* commInfo, MessageHolder message){

    return write(commInfo->communication_fd, message.fullMessage, strlen(message.fullMessage)) == -1 ? false : true;
}

/* *
 * UART veya Socket aracaligiyla mesaj verisi aliniyor ve Parse ediliyor.
 * */
void ReceivePackage( CommunicationInfo* commInfo, MessageHolder* responseMessage )
{

    int timeOut = 0; // Cevap 5 sn icinde gelmezse timeout olsun

    while(timeOut < 5){

        // Veriler aliniyor
        if (commInfo->communication_fd != -1)
        {
            char receivedMessage[256];

            int receivedMessageLength = read(commInfo->communication_fd, (void*)receivedMessage, 255);

            if ( receivedMessageLength > 0 ) {

                receivedMessage[receivedMessageLength] = '\0';
                printf("INFO - Message [%s], [%i] bytes read\n\n", receivedMessage, receivedMessageLength );

                ParseResponse(receivedMessage, responseMessage);        // veriler ParseResponse ile ayristiriliyor.
                break;
            }
        }

        sleep(1);
        timeOut++;
    }// while(timeOut < 5)

    if (timeOut >= 5 )
        printf("| WARNING - Timeout Occured, No coming message from server!\n");

}// ReceivePackage()

/* *
 * UART veya Socket aracaligiyla okunan veriler uygun formatlar icin ayristiriliyor.
 * */
void ParseResponse( char* inputMessage, MessageHolder* parsedMessage)
{

    strcpy(parsedMessage->fullMessage,inputMessage);

    char *ptr, *nptr, *arg, *cmd;

    // inputMessage'ı kopyala
    nptr = strdup(inputMessage);

    // ':' karakterini ayırıyoruz
    ptr = strtok(nptr,":");


    while(ptr!=NULL){

        // Boslugu ayiriyoruz.
        cmd = strtok(ptr," ");
        strcpy(parsedMessage->command, cmd);

        // Boslugu ayiriyoruz.
        arg = strtok(NULL," ");

        if(arg!=NULL)       strcpy(parsedMessage->argument, arg);

        ptr=strtok(NULL,":");

    }// end while

    DisplayResponseMessage(*parsedMessage);
}

/* *
 * Gonderilecek olan mesajin hazirlandıgı kısımdır.
 * */
void PrepareRequest( char* inputMessage, int argument, MessageHolder* preparedMessage)
{
    if ( !strcmp(inputMessage, "sensorDurum") ||
         !strcmp(inputMessage, "sensorSayi" ) ||
         !strcmp(inputMessage, "surucuDurum") ||
         !strcmp(inputMessage, "close" ) ||
         !strcmp(inputMessage, "sensorList"))
    {
        sprintf(preparedMessage->fullMessage, "%s:", inputMessage);
        strcpy(preparedMessage->command,inputMessage);
        strcpy(preparedMessage->argument, "-1");
    }
    else if ( !strcmp(inputMessage, "sensorTip") ||
              !strcmp(inputMessage, "surucu"   ))
    {

        sprintf(preparedMessage->fullMessage, "%s %d:", inputMessage, argument);
        strcpy(preparedMessage->command,inputMessage);
        sprintf(preparedMessage->argument, "%d", argument);
    }
} // end PrepareRequest()

/* *
 * Serverdan alinan verilerin uygun parse işlemlerinden sonra ekrana yazildigi kisimdir.
 * */
void DisplayResponseMessage(MessageHolder responseMessage)
{
    if(!strcmp(responseMessage.command,"sensorDurum"))
        for (int i = 0 ; i< NUMBER_OF_SENSOR ;i++)
            printf("| Sensor: [%d], Value: [%s]\n", i+1 ,(atoi(responseMessage.argument) & (1 >> i)) ? "ON" : "OFF");

    else if (!strcmp(responseMessage.command,"sensorSayi"))
        printf("| Total Number of Sensor at System: [%s]\n", responseMessage.argument);

    else if (!strcmp(responseMessage.command,"surucu"))
        printf("| [%s] - Relay Status Change\n", responseMessage.argument );

    else if (!strcmp(responseMessage.command,"surucuDurum"))
        printf("| Relay: [%s]\n", (!strcmp(responseMessage.argument,"1")) ? "OFF" : "ON");

}

/* *
 * Server tarafindan sensor tipleri ve id'lerini ayristirma isleminden sonra ekrana yazdirir.
 * */
void DisplaySensorList( CommunicationInfo* commInfo )
{

    MessageHolder messageHolder;
    PrepareRequest( "sensorList", -1, &messageHolder );

    if (!RequestPackage( commInfo, messageHolder ) )
        printf("| ERROR - Request didn't be sent!\n");

    // Mesaj okunuyor ve  gerekli ayristirma islemi gerceklestiriliyor.
    while(1)
    {
        if (commInfo->communication_fd != -1)
        {
            char receivedMessage[256];

            int receivedMessageLength = read(commInfo->communication_fd, (void*)receivedMessage, 255);

            if ( receivedMessageLength > 0 ) {

                receivedMessage[receivedMessageLength] = '\0';

                char *messageWithoutEndDelimeter, *duplicatedMessage, *sensorType;

                // mesaj kopyalaniyor.
                duplicatedMessage = strdup(receivedMessage);

                // ':' karakteri ayristiriliyor
                messageWithoutEndDelimeter = strtok(duplicatedMessage,":");

                // Mesaj virgüle göre ayristiriliyor.
                sensorType = strtok( messageWithoutEndDelimeter,",");
                int sensorId = 0;

                // alinan sensor listesi ekrana yazdiriliyor.
                printf("| SENSOR LIST\n");
                while(sensorType!=NULL)
                {
                    printf("| [%d] : %s\n", sensorId++, sensorType);
                    sensorType = strtok(NULL,",");
                }// end while

                break;
            }// end if ( receivedMessageLength > 0 )
        }// end if (commInfo->communication_fd != -1)
        sleep(1);
    }
}

