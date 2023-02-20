
/* Protocol 5 (pipelining) allows multiple outstanding frames. The sender
 * may transmit up to MAX_SEQ frames without waiting for an ack. In addition,
 * unlike the previous protocols, the network layer is not assumed to have
 * a new packet all the time. Instead, the network layer causes a
 * network_layer_ready event when there is a packet to send.
 */

#include "protocol.h"
#include <iostream>
#include <queue>

using namespace std;

/* global variables */
//timer_status *t= (timer_status *) notset;

bool check = false;
bool NoTimer = false;

bool NetworkLayer = false;
int TimeOutFlag = 0;
queue<frame> physical;
packet NetworkLayer_Buffer[8];

// Wait for an event to happen and return its type in event (kindly check possible events in event_type in header file)
void wait_for_event(event_type *event) {
    if (NetworkLayer) {
        *event = network_layer_ready;
    } else if (TimeOutFlag == 1) {
        *event = timeout;
    } else {
        *event = frame_arrival;
    }
}

// Fetch a packet from the network layer for transmission on the channel
void from_network_layer(packet *p) {
    static int s = 0;
    for (char j = 0; j < 8; j++) {
        p->data[j] = NetworkLayer_Buffer[s].data[j];
    }
    s++;
    if (s == 8) {
        s = 0;
    }
}

// Deliver information (data) to the network layer. */
void to_network_layer(packet *p) {
    static int r = 0;
    for (char j = 0; j < 8; j++) {
        NetworkLayer_Buffer[r].data[j] = p->data[j];
    }
    r++;
    if (r == 8) {
        r = 0;
    }
}

// Go get frame from the physical layer and copy it to r.
void from_physical_layer(frame *r) {
    *r = physical.front();
    physical.pop();
    cout << "receiving frame number # " << r->seq << endl;

}

// Pass the frame to the physical layer for transmission
void to_physical_layer(frame *s) {
    physical.push(*s);
    cout << "sending frame number #" << s->seq << endl;

}

// Start the clock running and enable the timeout event.
void start_timer(seq_nr k) {
    cout << "Timer started for " << k << endl;

}

// Stop the clock and disable the timeout event.
void stop_timer(seq_nr k) {
    cout << "Timer stopped for " << k << endl;

}

// Allow the network layer for network_layer_ready event
void enable_network_layer(void) {
    NetworkLayer = true;
}

// prevent the network layer from a network_layer_ready event
void disable_network_layer(void) {
    NetworkLayer = false;
}

static bool between(seq_nr a, seq_nr b, seq_nr c) {
    // Return true if (a <=b < c circularly; false otherwise.
    if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
        return (true);
    else
        return (false);
}

static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[]) {
    /* Construct and send a data frame. */
    frame s;                                            /* declaring a frame variable */
    s.info = buffer[frame_nr];                          /* insert packet into frame */
    s.seq = frame_nr;                                    /* insert sequence number into frame */
    s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);  /* piggyback ack */
    to_physical_layer(&s);                              /* transmit the frame */
    start_timer(frame_nr);                              /* start the timer running */
}

void simulate_protocol5(void) {
    seq_nr next_frame_to_send;  /* MAX_SEQ > 1; used for outbound stream */
    seq_nr ack_expected;        /* oldest frame as yet unacknowledged */
    seq_nr frame_expected;      /* next frame expected on inbound stream */
    frame r;                    /* scratch variable */
    packet buffer[MAX_SEQ + 1]; /* buffers for the outbound stream */
    seq_nr nbuffered;           /* current output buffers  */
    seq_nr i;                   /* used to index into the buffer array */
    event_type event;

    /* allow network_layer_ready events */
    ack_expected = 0;       /* next ack expected inbound */
    next_frame_to_send = 0; /* next frame going out */
    frame_expected = 0;     /* number of frame expected inbound */
    nbuffered = 0;          /* initially no packets are buffered */

    int z = 16;
    /*
 the next while loop is to limit outcomes to certain value to be able to visualize output, check visualization method
    and it can be ignored by choosing 3 at visualization method (normal flow)
  */
    while (z || NoTimer) {
        if (z % 3 == 0) {
            disable_network_layer();
            if (z % 9 == 0) {
                if (check)
                    TimeOutFlag = 1;
                if (!check)
                    TimeOutFlag = 0;
            }
        } else
            enable_network_layer();
        z--;

        r.seq = next_frame_to_send;
        r.ack = ack_expected;
        r.kind = frame_kind::data;
        wait_for_event(&event);

        switch (event) {
            case network_layer_ready: /* the network layer has a packet to send */
                /* Accept, save, and transmit a new frame. */
                from_network_layer(&buffer[next_frame_to_send]);       /* fetch new packet */
                nbuffered = nbuffered + 1;                             /* expand the sender's window */
                send_data(next_frame_to_send, frame_expected, buffer); /* transmit the frame */
                inc(next_frame_to_send);                               /* advance sender's upper window edge */
                break;

            case frame_arrival: /* a data or control frame has arrived */
                cout<< "\n********************FRAME ARRIVED SUCCESSFULLY********************\n"<< endl;
                from_physical_layer(&r); // get next frame from physical layer

                if (r.seq == frame_expected) {
                    // Frames are accepted only in order.
                    to_network_layer(&r.info); // pass packet to network layer
                    inc(frame_expected);       // increment lower edge of receiver's window
                }


                while (between(ack_expected, r.seq, next_frame_to_send)) {
                    // Handle piggybacked ack.
                    nbuffered = nbuffered - 1; // available buffers decremented
                    stop_timer(ack_expected);  // frame arrived intact; stop timer
                    inc(ack_expected);         // contract sender's window
                }
                break;

            case cksum_err:; //we can ignore bad frames
                break;

            case timeout:
                // trouble, retransmit all outstanding frames


                cout<< "\n*******************OOPS!TIMEOUT ,RETRANSMIT*******************\n"<< endl;
                next_frame_to_send = ack_expected; // start retransmitting here
                for (i = 1; i <= nbuffered; i++) {
                    send_data(next_frame_to_send, frame_expected, buffer); // resend 1 frame
                    inc(next_frame_to_send);                               // prepare to send the next one
                }
                TimeOutFlag = 0;
        }

        if (nbuffered < MAX_SEQ)
            enable_network_layer();
        else
            disable_network_layer();

    }

}



/*
this method selects the way you want to visualize the flow of the protocol, if you want
 TESTCASE 1: where there is possible TIMEOUT
 TESTCASE 2: FRAMES ARRIVAL ONLY
 TESTCASE 3:NO CONTROL over the previous TESTCASES (NORMAL FLOW) [looks like TESTCASE 1 but with infinite outputs]
 */
void visualizationMethod() {
    int y;
    bool condition = true;
    while (condition) {
        cout << "Hello! you want to visualize protocol on :\n"
                "1-POSSIBLE TIMED OUT ACK FRAMES \n"
                "2-ON ARRIVAL ONLY\n"
                "3-FOR INFINITE OUTPUT VISUALIZATION WITH POSSIBLE TIMEOUTS\n";
        cin >> y;
        if (y == 1) {
            check = true;
            condition = false;
        } else if (y == 2) {
            check = false;
            condition = false;
        } else if (y == 3) {
            check = true;
            condition = false;
            NoTimer = true;
        }
    }

}



int main()
{

    visualizationMethod();

    char x;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            cin >> x;
            NetworkLayer_Buffer[i].data[j] = x;
        }
    }
   simulate_protocol5();

}
