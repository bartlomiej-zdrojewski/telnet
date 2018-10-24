#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#if defined(WIN32)

    #include <winsock2.h>
    #include <ws2tcpip.h>

    #define CLOSE_SOCKET        closesocket
    uint8_t WindowsSocketsInitialised = 0;

    const char *inet_ntop ( int af, const void *src, char *dst, socklen_t size );

#elif defined(unix) || defined(__unix) || defined(__unix__)

    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>

    #define SOCKET              int
    #define CLOSE_SOCKET        close

#endif

// #define SOCKET_PROTOCOL_IPV4
#define SOCKET_PROTOCOL_IPV6
#define SOCKET_BUFFER_SIZE                      1024
#define SOCKET_CONNECTIONS_LIMIT                16

#define SERVER_STATUS_SAFE_TERMINATION          0
#define SERVER_STATUS_RUNNING                   1
#define SERVER_STATUS_WINDOWS_SOCKETS_ERROR     2
#define SERVER_STATUS_SOCKET_ERROR              3
#define SERVER_STATUS_SOCKET_OPTION_ERROR       4
#define SERVER_STATUS_BIND_ERROR                5
#define SERVER_STATUS_LISTEN_ERROR              6
#define SERVER_WELCOME_MESSAGE                  "Connected to Telnet Server\r\n> "
#define SERVER_CONNECTIONS_LIMIT_MESSAGE        "Sorry, the connections limit has been reached. You'll be disconnected now.\r\n"
#define SERVER_BUFFER_OVERFLOW_MESSAGE          "ERROR: Buffer has been overflowed. Command will not be processed.\r\n> "

uint8_t ServerStatus = 1;

void disconnectClient ( SOCKET ClientSockets [], uint16_t ClientIndex );
void onTerminate ( );

int main ( int argc, char *argv[] ) {

    atexit( onTerminate );
    srand( (unsigned int) time( NULL ) );

    int PortNumber = 8192 + rand() % ( 65535 - 8192 );

    for ( uint8_t i = 1; i < argc; i++ ) {

        if ( strcmp( argv[i], "-p" ) == 0 ) {

            ++i;

            if ( i < argc ) {

                sscanf( argv[i], "%d", &PortNumber ); } }

        /*
        else if ( strcmp( argv[i], "-VARIABLE" ) == 0 ) {

            ++i;

            if ( i < argc ) {

                sscanf( argv[i], "%d", &VARIABLE ); } }
        */

        }

    if ( PortNumber < 1024 ) {

        PortNumber = 8192 + rand() % ( 65535 - 1024 ); }

    if ( PortNumber > 65535 ) {

        PortNumber = 8192 + rand() % ( 65535 - 1024 ); }

    puts(   "-------------------------" );
    printf( "Telnet Server\n" );
    printf( "Port number: %d\n", PortNumber );
    puts(   "-------------------------" );
    printf( "Launching server..." );

    #if defined(WIN32)

        WSADATA Data;

        if( WSAStartup( 0x202, &Data ) != 0 ) {

            printf( " Error on WSAStartup()\n" );

            ServerStatus = SERVER_STATUS_WINDOWS_SOCKETS_ERROR;
            return ServerStatus; }

        WindowsSocketsInitialised = 1;

    #endif

    #if defined(SOCKET_PROTOCOL_IPV4)

        SOCKET SocketHandle = socket( AF_INET, SOCK_STREAM, 0 );
        struct sockaddr_in SocketAddress;
        int SocketOption = 1;

        SocketAddress.sin_family = AF_INET;
        SocketAddress.sin_addr.s_addr = INADDR_ANY;
        SocketAddress.sin_port = htons( (uint16_t) PortNumber );

    #elif defined(SOCKET_PROTOCOL_IPV6)

        SOCKET SocketHandle = socket( AF_INET6, SOCK_STREAM, 0 );
        struct sockaddr_in6 SocketAddress;
        int SocketOption = 1;

        SocketAddress.sin6_family = AF_INET6;
        SocketAddress.sin6_addr = in6addr_any;
        SocketAddress.sin6_port = htons( (uint16_t) PortNumber );

    #endif

    if ( SocketHandle == -1 ) {

        printf( " Error on socket()\n" );
        
        ServerStatus = SERVER_STATUS_SOCKET_ERROR;
        return ServerStatus; }

    if ( setsockopt( SocketHandle, SOL_SOCKET, SO_REUSEADDR, (char*) &SocketOption, sizeof( SocketOption ) ) == -1 ) {

        printf( " Error on setsockopt()\n" );
        
        ServerStatus = SERVER_STATUS_SOCKET_OPTION_ERROR;
        return ServerStatus; }

    if ( bind( SocketHandle, (struct sockaddr*) &SocketAddress, sizeof(SocketAddress) ) == -1 ) {

        printf( " Error on bind()\n" );
        
        ServerStatus = SERVER_STATUS_BIND_ERROR;
        return ServerStatus; }

    if ( listen( SocketHandle, SOCKET_CONNECTIONS_LIMIT ) == -1 ) {

        printf( " Error on listen()\n" );
        
        ServerStatus = SERVER_STATUS_LISTEN_ERROR;
        return ServerStatus; }

    printf( " Done\n" );
    printf("Waiting for connections...\n");

    fd_set Clients;
    SOCKET ClientSockets [SOCKET_CONNECTIONS_LIMIT];
    char ClientBuffers [SOCKET_CONNECTIONS_LIMIT] [SOCKET_BUFFER_SIZE];
    int16_t ClientBuffersSize [SOCKET_CONNECTIONS_LIMIT];
    SOCKET LastSocket = SocketHandle;

    for ( uint16_t i = 0; i < SOCKET_CONNECTIONS_LIMIT; i++ ) {

        ClientSockets[i] = 0;
        ClientBuffersSize[i] = 0; }

    while ( ServerStatus == SERVER_STATUS_RUNNING ) { // Main loop

        FD_ZERO( &Clients );
        FD_SET( SocketHandle, &Clients );

        for ( uint16_t i = 0 ; i < SOCKET_CONNECTIONS_LIMIT; i++ ) {

            if ( ClientSockets[i] > 0 ) {

                FD_SET( ClientSockets[i] , &Clients ); }

            if ( ClientSockets[i] > LastSocket ) {

                LastSocket = ClientSockets[i]; } }

        select( (int) LastSocket + 1, &Clients, NULL, NULL, NULL ); // TODO Set timeout

        if ( FD_ISSET( SocketHandle, &Clients ) ) { // New client connection

            #if defined(SOCKET_PROTOCOL_IPV4)

                struct sockaddr_in ClientSocketAddress;
                unsigned int ClientSocketAddressLength = sizeof( ClientSocketAddress );

            #elif defined(SOCKET_PROTOCOL_IPV6)

                struct sockaddr_in6 ClientSocketAddress;
                unsigned int ClientSocketAddressLength = sizeof( ClientSocketAddress );

            #endif

            SOCKET ClientSocketHandle = accept( SocketHandle, (struct sockaddr*) &ClientSocketAddress, &ClientSocketAddressLength );

            if ( ClientSocketHandle == -1 ) {

                continue; }

            #if defined(SOCKET_PROTOCOL_IPV4)

                char ClientSocketAddressString [256];

                inet_ntop( AF_INET, &ClientSocketAddress.sin_addr, ClientSocketAddressString, sizeof( ClientSocketAddressString ) );
                printf( "Connecting to %s:%d...", ClientSocketAddressString, ntohs( ClientSocketAddress.sin_port ) );

            #elif defined(SOCKET_PROTOCOL_IPV6)

                char ClientSocketAddressString [256];

                inet_ntop( AF_INET6, &ClientSocketAddress.sin6_addr, ClientSocketAddressString, sizeof( ClientSocketAddressString ) );
                printf( "Connecting to [%s]:%d...", ClientSocketAddressString, ntohs( ClientSocketAddress.sin6_port ) );

            #endif

            int16_t ClientIndex = -1;

            for ( uint16_t i = 0; i < SOCKET_CONNECTIONS_LIMIT; i++ ) {

                if ( ClientSockets[i] == 0 ) {

                    ClientSockets[i] = ClientSocketHandle;
                    ClientIndex = i;

                    break; } }

            if ( ClientIndex != -1 ) { // Connection accepted

                char Message [] = SERVER_WELCOME_MESSAGE;
                send( ClientSocketHandle, Message, (int) strlen( Message ), 0 );

                printf( " Done\n" ); }

            else { // Connection refused

                char Message [] = SERVER_CONNECTIONS_LIMIT_MESSAGE;
                send( ClientSocketHandle, Message, (int) strlen( Message ), 0 );
                CLOSE_SOCKET( ClientSocketHandle );

                printf( " Error, connections limit reached\n" ); } }

        for ( uint16_t i = 0; i < SOCKET_CONNECTIONS_LIMIT; i++ ) { // Connected clients

            if ( FD_ISSET( ClientSockets[i], &Clients ) ) { // On client activity

                char Buffer [SOCKET_BUFFER_SIZE];
                int BufferSize = (int) recv( ClientSockets[i], Buffer, SOCKET_BUFFER_SIZE, 0 );

                if ( BufferSize <= 0 ) { // On client connection closed

                    disconnectClient( ClientSockets, i ); }

                else { // On client message

                    if ( ( ClientBuffersSize[i] + BufferSize ) > SOCKET_BUFFER_SIZE ) { // On client buffer overflow

                        char Message [] = SERVER_BUFFER_OVERFLOW_MESSAGE;
                        send( ClientSockets[i], Message, (int) strlen( Message ), 0 );

                        ClientBuffersSize[i] = 0; }

                    else { // On client buffer append

                        strncpy( ClientBuffers[i] + ClientBuffersSize[i], Buffer, (size_t) BufferSize );

                        ClientBuffersSize[i] += BufferSize;
                        ClientBuffers[i][ClientBuffersSize[i]] = '\0';

                        char * LineEnd = strstr( ClientBuffers[i], "\r\n" );

                        if ( LineEnd != NULL ) {

                            *LineEnd = '\0';

                            ClientBuffersSize[i] = 0; }

                        else {

                            LineEnd = strstr( ClientBuffers[i], "\n" );

                            if ( LineEnd != NULL ) {

                                *LineEnd = '\0';

                                ClientBuffersSize[i] = 0; } }

                        if ( LineEnd != NULL ) { // On client request

                            // TODO Your code begin

                            if ( strcmp( ClientBuffers[i], "exit" ) == 0 ) {

                                disconnectClient( ClientSockets, i ); }

                            else if ( strcmp( ClientBuffers[i], "terminate" ) == 0 ) {

                                ServerStatus = SERVER_STATUS_SAFE_TERMINATION; }

                            else {

                                char Response [SOCKET_BUFFER_SIZE];
                                Response[0] = '\0';

                                strcpy( Response, "You have sent me a message starting with: \"" );
                                strncpy( Response + 43, ClientBuffers[i], 24 );
                                strcpy( Response + 43 + ( strlen( ClientBuffers[i] ) < 24 ? strlen( ClientBuffers[i] ) : 24 ), "\".\r\n> " );

                                send( ClientSockets[i], Response, (unsigned int) strlen( Response ), 0 ); }

                            // TODO Your code end

                            } } } } }

        if ( ServerStatus == 1 ) { // Additional server routine

            // TODO Additional server routine

            }

        else { // Routine before server termination

            for ( uint16_t i = 0; i < SOCKET_CONNECTIONS_LIMIT; i++ ) {

                if ( ClientSockets[i] != 0 ) {

                    disconnectClient( ClientSockets, i ); } } } }

    return ServerStatus; }

void disconnectClient ( SOCKET ClientSockets [], uint16_t ClientIndex ) {

    #if defined(SOCKET_PROTOCOL_IPV4)

        struct sockaddr_in ClientSocketAddress;
        unsigned int ClientSocketAddressLength = sizeof( ClientSocketAddress );

    #elif defined(SOCKET_PROTOCOL_IPV6)

        struct sockaddr_in6 ClientSocketAddress;
        unsigned int ClientSocketAddressLength = sizeof( ClientSocketAddress );

    #endif

    getpeername( ClientSockets[ClientIndex], (struct sockaddr*) &ClientSocketAddress, &ClientSocketAddressLength );

    #if defined(SOCKET_PROTOCOL_IPV4)

        char ClientSocketAddressString [256];

        inet_ntop( AF_INET, &ClientSocketAddress.sin_addr, ClientSocketAddressString, sizeof( ClientSocketAddressString ) );
        printf( "Disconnecting from %s:%d...", ClientSocketAddressString, ntohs( ClientSocketAddress.sin_port ) );

    #elif defined(SOCKET_PROTOCOL_IPV6)

        char ClientSocketAddressString [256];

        inet_ntop( AF_INET6, &ClientSocketAddress.sin6_addr, ClientSocketAddressString, sizeof( ClientSocketAddressString ) );
        printf( "Disconnecting from [%s]:%d...", ClientSocketAddressString, ntohs( ClientSocketAddress.sin6_port ) );

    #endif

    CLOSE_SOCKET( ClientSockets[ClientIndex] );
    ClientSockets[ClientIndex] = 0;

    printf( " Done\n" ); }

void onTerminate ( ) {

    #if defined(WIN32)

        if ( WindowsSocketsInitialised != 0 ) {

            WSACleanup(); }

    #endif

    printf( "Server terminated with status %d\n", ServerStatus );
    printf( "-------------------------" );

    #if defined(unix) || defined(__unix) || defined(__unix__)

        printf( "\n" );

    #endif

    }
