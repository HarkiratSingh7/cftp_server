/*
    Author: Harkirat Singh
*/

#ifndef _HASH_MAP_UNIVERSE_H
#define _HASH_MAP_UNIVERSE_H

#include <stddef.h>

/*
    Implementation of my hash tables
*/

#define HASH_TABLE_INTIAL_SIZE 21
#define DEFAULT_LOAD_FACTOR 0.75f

/* Hash Error Codes (Bits) */
#define HASH_ENTRY_FAILED 0x0
#define HASH_ENTRY_SUCCESS 0x1
#define HASH_ENTRY_COLLISION 0x2
#define HASH_TABLE_RESIZED 0x4
#define HASH_FREED 0x8
#define HASH_ENTRY_UPDATED 0x10

/*
    Hash Table Elements
        - Follows a linked list structure to resolve collision
*/
struct hash_table_elem
{
    struct hash_table_elem *next, *prev;
    char *key;
    void *address;
};

/*
    Hash Table
        - To store the root hash table element along with properties
*/
struct hash_table
{
    size_t length;
    struct hash_table_elem **table;
    float load_factor;
    int elem_count;
};

/*  FUNCTION DECLARATIONS  */

/*
    Allocates memory for hash table structure and hash table element pointer
    table,and initializes to default values.
*/
struct hash_table *create_hash_table(void);

/*
    De-allocates the hash table structure.
    Note: This must be handled accordingly with the higher implementations.
*/
int free_hash_table(struct hash_table *hashtable);

/*
    Check if key exists in the hash set
*/
int check_key_exists(struct hash_table *hashtbl, const char *keyname);

/*
    Hash table entry requires the object to be allocated already,
    it only focuses on hash table element insertion.
*/
int insert_entry(struct hash_table *hashtbl, const char *keyname, void *addr);

/*
    Returns pointer to the address, the high level calling structure must
    correctly interpret the type.
*/
void *get_ptr_to_value_by_key(struct hash_table *hashtbl, const char *keyname);

/*
    Delete the respective hash table element from hash table,
    not responsible to clear the address value.
*/
int delete_entry(struct hash_table *hashtable, const char *keyname);

/* Helpers */
#define HASHSET_INSERT(set, item) insert_entry(set, item, NULL)

#endif  // _HASH_MAP_UNIVERSE_H
