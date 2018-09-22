#include <stdio.h>
#include <assert.h>

#include "hldc.h"

bool test(uint8_t *buff, size_t buff_len, size_t data_len, size_t *num_packets, bool verbose) {
    hldc_state_t state;
    size_t i, j;

    size_t ret_len = 0;
    bool found = false;

    *num_packets = 0;

    hldc_init(&state);
    if(verbose) {
        printf("state(in_packet=%5s, one_count=%3zu, buff_idx=%3zu)\n",
                state.in_packet ? "true" : "false",
                state.one_count,
                state.buff_idx);
    }

    for(i = 0; i < buff_len; i++) {
        found = hldc_execute(&state, (buff[i] == 1 ? 1.0 : -1.0), &ret_len);
        if(verbose) {
            printf("%3zu state(in_packet=%5s, one_count=%3zu, buff_idx=%3zu) %d = %5s / %3zu\n",
                    i,
                    state.in_packet ? "true" : "false",
                    state.one_count,
                    state.buff_idx,
                    buff[i],
                    found ? " true" : "false",
                    ret_len);
        }
        if(found) {
            *num_packets += 1;
            if(verbose) {
                printf("ret: [");
                for(j = 0; j < ret_len; j++) {
                    if(j > 0 && j % 8 == 0) {
                        printf("\n      ");
                    }
                    printf("%d, ", state.samps[j] > 0 ? 1 : 0);
                }
                printf("] pkt %zu, %zu samps\n", *num_packets, ret_len);
            }
        }
    }

    return ret_len == data_len;
}

int main(void) {
    size_t num_packets;

    // Simple test, send one byte of all zeros, no stuffing needed
    uint8_t test_01[] = {0,1,1,1, 1,1,1,0,
                         0,0,0,0, 0,0,0,0,
                         0,1,1,1, 1,1,1,0};
    assert(test(test_01, sizeof(test_01), 8, &num_packets, false));
    assert(num_packets == 1);

    // Send two bytes, with one needing stuffing
    uint8_t test_02[] = {0,1,1,1, 1,1,1,0,
                         0,1,1,1, 1,1,0,1,0,
                         0,0,0,0, 0,0,0,0,
                         0,1,1,1, 1,1,1,0};
    assert(test(test_02, sizeof(test_02), 16, &num_packets, false));
    assert(num_packets == 1);

    // Send two packets with stuffing, make sure we get both
    uint8_t test_03[] = {0,1,1,1, 1,1,1,0,
                         0,1,1,1, 1,1,0,1,0,
                         0,0,0,0, 0,0,0,0,
                         0,1,1,1, 1,1,1,0,
                         0,1,1,1, 1,1,0,1,0,
                         0,0,0,0, 0,0,0,0,
                         0,1,1,1, 1,1,1,0};
    assert(test(test_03, sizeof(test_03), 16, &num_packets, false));
    assert(num_packets == 2);

    // Send an empty packet, make sure we don't get it
    uint8_t test_04[] = {0,1,1,1, 1,1,1,0,
                         0,1,1,1, 1,1,1,0};
    assert(test(test_04, sizeof(test_04), 0, &num_packets, false));
    assert(num_packets == 0);

    // Send a packet that shares zeros for the flag, make sure we get it
    uint8_t test_05[] = {1,1,1, 1,1,1,
                         0,1,1,1, 1,1,1,0,
                         0,1,1,1, 1,1,0,1,0,
                         0,1,1,1, 1,1,1
                         };
    assert(test(test_05, sizeof(test_05), 8, &num_packets, false));
    assert(num_packets == 1);

    // Missing the trailing 0, should be ok here
    uint8_t test_06[] = {0,1,1,1, 1,1,1,0,
                         0,1,1,1, 1,1,0,1,0,
                         0,1,1,1, 1,1,1
                         };
    assert(test(test_06, sizeof(test_06), 8, &num_packets, false));
    assert(num_packets == 1);

    return 0;
}

