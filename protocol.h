/**
 * @file protocol.h
 * @brief Protocol for the Vegosh key-value store.
 *
 * Wire format:
 *   [1 byte opcode] [1 byte key_len] [1 byte val_len] [key_len bytes key] [val_len bytes value]
 *
 * Opcodes:
 *   0x01 - SET
 *   0x02 - GET
 *
 * Status codes:
 *   69 (SUCCESS)              - Operation completed successfully
 *   67 (KEY_NOT_FOUND)        - Key does not exist in the store
 *   68 (KEY_EXISTS_UPDATED)   - Key already existed, value was overwritten
 *   66 (MAX_KEY_LIMIT_REACHED)- Store is full, insertion rejected
 *   65 (DATA_CORRUPTION)      - CRC32 check failed
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define SUCCESS               69
#define KEY_NOT_FOUND         67
#define KEY_EXISTS_UPDATED    68
#define MAX_KEY_LIMIT_REACHED 66
#define DATA_CORRUPTION       65
#define INVALID_OPCODE        64

int initializevegosh(void);

/**
 * @brief Handles a SET request.
 *
 * Reads key_len, val_len, then exactly that many bytes for key and value.
 * Calls insert() and writes a single status byte back to the client.
 *
 * @param connfd File descriptor of the client connection.
 * @return 0 on success, -1 on error.
 */
int handle_insert(int connfd);

/**
 * @brief Handles a GET request.
 *
 * Reads key_len, then exactly that many bytes for the key.
 * On success, writes [SUCCESS][value_len][value] back to the client.
 * On failure, writes [KEY_NOT_FOUND].
 *
 * @param connfd File descriptor of the client connection.
 * @return 0 on success, -1 on error.
 */
int handle_get(int connfd);

/**
 * @brief Reads the opcode byte and dispatches to handle_insert or handle_get.
 *
 * @param connfd File descriptor of the client connection.
 * @return 0 on success, -1 on unknown opcode or handler error.
 */
int parser(int connfd);

#endif
