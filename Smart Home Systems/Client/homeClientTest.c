#include <stdio.h>
#include <stdlib.h>

#include <signal.h>         // SIGINT

#include <sys/types.h>
#include <sys/select.h>

#include <unistd.h>

#include <ctype.h>          // tolower()

#include "CommunicationAPI.h"

// TODO: Aradaki bağlantı kapatılacak fakat tekrar bağlandığında sorun oluyor
#define btoa(x) ((x)?"true":"false")


/***    FONKSİYON PROTOTİPLERİ    ***/
void DisplayCommunicationTypeMenu();
void DisplayOperationMenu();
void ManageChoiceFromMenu();

void signal_handler(int no);

MessageHolder       structMessageHolder;    // Icerisinde Mesaj ait komut, arguman ve tam mesaji tutar.
CommunicationInfo   structCommInfoHolder;   // Icerisinde File descriptor, ip adresi ve port numarasini tutar.

/* *
 * Main Fonksiyonu
 * */
int main() {

    // SIGINT (CTRL + C) sinyalini kontrol icin gerekli ayarlamalar yapiliyor.
    struct sigaction new_action, old_action;
    new_action.sa_handler=signal_handler;
    new_action.sa_flags=SA_NODEFER | SA_ONSTACK;
    sigaction(SIGINT, &new_action, &old_action);

    // menu secimi
    int menuChoice = 0;

    DisplayCommunicationTypeMenu();
    scanf("%d", &menuChoice);

    while ((getchar()) != '\n'); // Olasi "\n" karakterine bagli olasi terminal cikti hatalarini onlemek icin

    system("clear"); // terminali temizle


    switch (menuChoice) {

        // UART Baglantisi Secildi
        case 1: {

            // UART icin degiskenler ayarlaniyor.
            if (!ConnectWithUART(&structCommInfoHolder))
            {
                printf("| ERROR - Cannot init UART!\n");
                //printf("%s\n", btoa(ConnectWithUART(&structCommInfoHolder)));
            }
              


            else
            {
     
                while (1) {

                    DisplayOperationMenu();

                    scanf("%d", &menuChoice);
                    while ((getchar()) != '\n');      // Olasi "\n" karakterine bagli olasi terminal cikti hatalarini onlemek icin

                    system("clear");                  // Terminali temizle

                    ManageChoiceFromMenu(menuChoice); // Yapilan secime uygun islem gerceklesir.
                    printf("aefaf");
                    // UART ile server'a istegi yapilmasi
                    if (!RequestPackage(&structCommInfoHolder, structMessageHolder))
                    {
                        printf("| ERROR - Request didn't sent\n");
                        continue;
                    }

                    ReceivePackage(&structCommInfoHolder, &structMessageHolder);       // UART ile server'dan mesaj alinmasi
                } // end While(1)
            }
            break;
        } // end case 1

        case 2: {

            printf("| Please provide following information\n");
            printf("| Enter Server IP: ");
            scanf("%[^\n]%*c", structCommInfoHolder.ip);

            printf("| Enter Port Number: ");
            scanf("%d", &structCommInfoHolder.port);

            while ((getchar()) != '\n');            // Olasi "\n" karakterine bagli olasi terminal cikti hatalarini onlemek icin
            system("clear");                        // Terminali temizle

            if( ConnectWithSocket(&structCommInfoHolder) ) {     // Socket Baglantisinin olusturulmasi

                while (1) {

                    printf("\n\n| Server IP   : [%s]\n", structCommInfoHolder.ip);
                    printf("| Server Port : [%d]\n", structCommInfoHolder.port);

                    DisplayOperationMenu();
                    scanf("%d", &menuChoice);
                    while ((getchar()) !=
                           '\n');        // Olasi "\n" karakterine bagli olasi terminal cikti hatalarini onlemek icin

                    system("clear");                        // Terminali temizle

                    ManageChoiceFromMenu(menuChoice);       // Yapilan secime uygun islem gerceklesir.

                    if (menuChoice == 3) continue;

                    // Socket ile server'a istegi yapilmasi
                    if (!RequestPackage(&structCommInfoHolder, structMessageHolder)) {
                        printf("| ERROR - Request didn't sent\n");
                        continue;
                    }
                    ReceivePackage(&structCommInfoHolder,
                                   &structMessageHolder);  // Socket ile server'dan mesaj alinmasi

                } // end while(1)
            }
            break;
        } // end case 2
    } // end switch
    return 0;
} // end Main()


void DisplayCommunicationTypeMenu() {

    printf("\
           \n--------------------------------------------------\
           \n|                HOME AUTOMATION                 |\
           \n--------------------------------------------------\
           \n| Please choose one of the communication type    |\
           \n| 1. UART                                        |\
           \n| 2. SOCKET                                      |\
           \n--------------------------------------------------\
           \n| Enter a choice: ");
}

void DisplayOperationMenu() {

    printf("\
            \n--------------------------------------------------\
            \n|                OPERATION  MENU                 |\
            \n--------------------------------------------------\
            \n| 1. Current Sensor Values                       |\
            \n| 2. Total Number of Sensors                     |\
            \n| 3. Display All Sensors                         |\
            \n| 4. Change Status of Relay                      |\
            \n| 5. Display Current Status of Relay             |\
            \n| 6. Switch Automode                             |\
            \n| 7. Exit                                        |\
            \n--------------------------------------------------\
            \n| Enter a command: ");
}

void ManageChoiceFromMenu(int menuChoice) {

    if (menuChoice == 1)
        PrepareRequest("sensorDurum", -1, &structMessageHolder);

    else if (menuChoice == 2)
        PrepareRequest("sensorSayi", -1, &structMessageHolder);

    else if (menuChoice == 3)
        DisplaySensorList(&structCommInfoHolder);       // Serverdan aldıgı sensor bilgilerini ekrana basıyor.

    else if (menuChoice == 4) {

        int status = -1;

        printf("| Set Relay by typing '1'--> ON or '0' --> OFF\n");
        printf("| Enter a choice:");
        scanf("%d", &status);

        while ((getchar()) != '\n');

        // Ters lojik oldugu icin secim "1"'den cikarildi.
        PrepareRequest("surucu", (1-status), &structMessageHolder);

    } // end else if(menuChoice == 4)

    else if (menuChoice == 5)
        PrepareRequest("surucuDurum", -1, &structMessageHolder);

    else if (menuChoice == 6) {

        char inputSensorID[5];
        char inputSensorValue[5];

        DisplaySensorList(&structCommInfoHolder);   // Serverdan aldıgı sensor bilgilerini ekrana basıyor.
        printf("| Enter ID of Sensor:");

        scanf("%[^\n]%*c", inputSensorID);

        printf("| What Sensor Value opens the Relay? ( 1 or 0 ):");
        scanf("%[^\n]%*c", inputSensorValue);

        strcat(inputSensorID, inputSensorValue);
        sprintf(structMessageHolder.fullMessage,"autoMode on %d %d:",atoi(inputSensorID),atoi(inputSensorValue));

        RequestPackage(&structCommInfoHolder, structMessageHolder);

        char chExit;
        printf("| To Exit in 'AutoMod' press 'e': ");
        scanf("%[^\n]%*c", &chExit);

        if (tolower(chExit) == 'e')
            sprintf(structMessageHolder.fullMessage, "autoMode off:");
        system("clear");                                                    // Terminali temizle
    } // end else if(menuChoice == 6)

    else if (menuChoice == 7) {

        PrepareRequest("close", -1, &structMessageHolder);
        RequestPackage(&structCommInfoHolder,structMessageHolder);

        if (CloseConnection(&structCommInfoHolder)) {

            printf("| Application terminate!\n");
            exit(0);
        }
    }

} // ManageChoiceFromMenu

void signal_handler(int no)
{

    if(no == SIGINT || no == SIGKILL){

        printf("Closing Application...\n");

        PrepareRequest("close", -1, &structMessageHolder);
        RequestPackage(&structCommInfoHolder, structMessageHolder);

        sleep(4);
        close(structCommInfoHolder.communication_fd);

        exit(0);
    }
}