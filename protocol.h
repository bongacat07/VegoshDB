/**
 * @file protocol.h
 * @brief Protocol for the Vegosh key-value store.
 *        Keys are 16-byte identifiers, and values are 32-byte values.
 *        The status codes are:
 *          - 69: Success
 *          - 67: Key not found (denoted by -1)
 *          - 68: Key already exists and it has been updated (denoted by 1)
 *          - 66: Max key limit reached (denoted by -2)
 *          - 65: Data corruption detected by CRC 32 implementation
 *
 * Usage:
 *   initializevegosh() → insert() / get()
 */



#ifndef PROTOCOL_H
#define PROTOCOL_H

#define SUCCESS 69
#define KEY_NOT_FOUND 67
#define KEY_EXISTS_UPDATED 68
#define MAX_KEY_LIMIT_REACHED 66
#define DATA_CORRUPTION 65
int initializevegosh(void);

/**
 * @brief Inserts or updates a key-value pair.
 *
 * If the key already exists its value is overwritten without consuming an
 * additional slot. New insertions are rejected once MAX_KEYS is reached.
 *
 * @param key   Pointer to exactly 16 bytes of key data.
 * @param value Pointer to exactly 32 bytes of value data.
 * @return 0 on success, -1 if the table is full or the key cap is reached, 1 if the key already exists and is updated.
 *
 */
int handle_insert(int connfd);

/**
 * @brief Retrieves the value associated with a key.
 *
 *
 * @param key Pointer to exactly 16 bytes of key data.
 * @return 0 on success, -1 if the key is not found.
 */
int handle_get(int connfd);

int parser(int connfd);

#endif
