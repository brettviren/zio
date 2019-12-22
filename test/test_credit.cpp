// from zguide
// http://zguide.zeromq.org/page:all#Transferring-Files
// modified to work with recent CZMQ.

// first do:
// dd if=/dev/urandom of=testdata bs=1M count=1024


//  File Transfer model #3
//  
//  In which the client requests each chunk individually, using
//  command pipelining to give us a credit-based flow control.

#include <czmq.h>
#define CHUNK_SIZE  250000
#define PIPELINE    10

static void
client_actor (zsock_t* pipe, void *args)
{
    zsock_signal(pipe, 0);

    zsock_t *dealer = zsock_new (ZMQ_DEALER);
    assert(dealer);
    zsock_connect (dealer, "tcp://127.0.0.1:6000");

    //  Up to this many chunks in transit
    size_t credit = PIPELINE;
    
    size_t total = 0;       //  Total bytes received
    size_t chunks = 0;      //  Total chunks received
    size_t offset = 0;      //  Offset of next chunk request
    
    while (true) {
        while (credit) {
            //  Ask for next chunk
            zstr_sendm  (dealer, "fetch");
            zstr_sendfm (dealer, "%ld", offset);
            zstr_sendf  (dealer, "%ld", (long) CHUNK_SIZE);
            offset += CHUNK_SIZE;
            credit--;
        }
        zframe_t *chunk = zframe_recv (dealer);
        if (!chunk)
            break;              //  Shutting down, quit
        chunks++;
        credit++;
        size_t size = zframe_size (chunk);
        zframe_destroy (&chunk);
        total += size;
        if (size < CHUNK_SIZE)
            break;              //  Last chunk received; exit
    }
    zsys_debug ("%zd chunks received, %zd bytes\n", chunks, total);
    zsock_destroy(&dealer);
    zstr_send (pipe, "OK");
}

//  The rest of the code is exactly the same as in model 2, except
//  that we set the HWM on the server's ROUTER socket to PIPELINE
//  to act as a sanity check.

//  The server thread waits for a chunk request from a client,
//  reads that chunk and sends it back to the client:

static void
server_actor (zsock_t* pipe, void *args)
{
    zsock_signal(pipe, 0);

    FILE *file = fopen ("testdata", "r");
    assert (file);

    zsock_t *router = zsock_new (ZMQ_ROUTER);
    assert(router);
    //  We have two parts per message so HWM is PIPELINE * 2
    zsock_set_hwm (router, PIPELINE * 2);
    zsock_bind (router, "tcp://*:6000");
    zpoller_t* poller = zpoller_new(router, pipe, NULL);
    while (!zsys_interrupted) {
        void* which = zpoller_wait(poller, -1);
        if (!which or which == pipe) {
            break;
        }

        //  First frame in each message is the sender identity
        zframe_t *identity = zframe_recv (router);
        if (!identity)
            break;              //  Shutting down, quit
            
        //  Second frame is "fetch" command
        char *command = zstr_recv (router);
        assert (streq (command, "fetch"));
        free (command);

        //  Third frame is chunk offset in file
        char *offset_str = zstr_recv (router);
        size_t offset = atoi (offset_str);
        free (offset_str);

        //  Fourth frame is maximum chunk size
        char *chunksz_str = zstr_recv (router);
        size_t chunksz = atoi (chunksz_str);
        free (chunksz_str);

        //  Read chunk of data from file
        fseek (file, offset, SEEK_SET);
        byte *data = (byte*)malloc (chunksz);
        assert (data);

        //  Send resulting chunk to client
        size_t size = fread (data, 1, chunksz, file);
        zframe_t *chunk = zframe_new (data, size);
        zframe_send (&identity, router, ZFRAME_MORE);
        zframe_send (&chunk, router, 0);
    }
    fclose (file);
    zsock_destroy(&router);
}

//  The main task starts the client and server threads; it's easier
//  to test this as a single process with threads, than as multiple
//  processes:

int main (void)
{
    zactor_t* server = zactor_new(server_actor, NULL);
    assert(server);
    zactor_t* client = zactor_new(client_actor, NULL);
    assert(client);

    //  Loop until client tells us it's done
    zsys_debug("waiting for client");
    char *str = zstr_recv (client);
    free (str);

    zsock_signal(server, 0);

    zsys_debug("destroy client");
    zactor_destroy(&client);

    zsys_debug("destroy server");
    zactor_destroy(&server);    
    return 0;
}
