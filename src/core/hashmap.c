#include "hashmap.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#define FNV_1A_OFFSET_BASIS 0xCBF29CE484222325
#define FNV_1A_PRIME 0x100000001B3
#define FNV_1A_CHECKSUM_LENGTH 0x8

/*
    Internal functions
*/

/* Hashing function */
static uint64_t fnv1a_string_hash(const uint8_t *str);

/* Function to get pointer to the hash table element structure. */
static struct hash_table_elem *get_ptr_to_elem_by_key(
    struct hash_table *hashtable,
    const char *keyname);

/* Function to check if hash table needs reallocation using the load factor.*/
static uint64_t check_grow(struct hash_table *hashtable);

/* Function to allocate an instance of hash table element structure. */
static struct hash_table_elem *create_hash_table_elem(const char *key,
                                                      void *address);

/* Start of function definitions*/

static uint64_t fnv1a_string_hash(const uint8_t *str)
{
    uint64_t hash = FNV_1A_OFFSET_BASIS;
    while (*str)
    {
        hash = (hash ^ *str) * FNV_1A_PRIME;
        str++;
    }

    return hash;
}

/* Function to allocate and initialize a hash table structure. */
struct hash_table *create_hash_table()
{
    struct hash_table *ptr =
        (struct hash_table *)malloc(sizeof(struct hash_table));

    if (!ptr)
    {
        ERROR("Failed to allocate memory for hash table object");
        return NULL;
    }

    /* Initialie hash table element strucuture (ptr) table to initial size. */
    ptr->table =
        calloc(HASH_TABLE_INTIAL_SIZE, sizeof(struct hash_table_elem *));

    ptr->length = HASH_TABLE_INTIAL_SIZE;
    ptr->elem_count = 0;
    ptr->load_factor = DEFAULT_LOAD_FACTOR;

    return ptr;
}

static uint64_t check_grow(struct hash_table *hashtable)
{
    if (!hashtable)
    {
        ERROR("Invalid hashtable object");
        return HASH_ENTRY_FAILED;
    }

    /*
       (TOTAL_ELEMENTS) / (ALLOCATED_SIZE_OF_TABLE) should be 0.7 to 0.8 .

       If the ratio of total elements inserted to earlier allocated elements
       is greater than the load factor then double the allocated element size.
       Note: Load factor of 0.7 to 0.8 is considered good. This should always
       be tested in the unit test of hash table.
     */
    if ((hashtable->length * hashtable->load_factor) <=
        hashtable->elem_count + 1)
        return hashtable->length * 2;

    /* When resize is not required then simply return zero. */
    return 0;
}

int check_key_exists(struct hash_table *hashtbl, const char *keyname)
{
    return get_ptr_to_elem_by_key(hashtbl, keyname) ? 1 : 0;
}

int insert_entry(struct hash_table *hashtable,
                 const char *keyname,
                 void *address)
{
    if (!hashtable)
    {
        ERROR("Invalid hashing table");
        return HASH_ENTRY_FAILED;
    }

    if (!keyname)
    {
        ERROR("Invalid string object");
        return HASH_ENTRY_FAILED;
    }

    /*
       Calculate the hash using FNV-1a function.
       Note: Collision is mainly affected by length, but
       optimal value of constants for FNV-1a should be used.
     */
    uint64_t keyhash =
        fnv1a_string_hash((const uint8_t *)keyname) % hashtable->length;

    /* Check if any element is with same keyname is allocated already. */
    struct hash_table_elem *ptr = get_ptr_to_elem_by_key(hashtable, keyname);

    if (ptr)
    {
        /* If element is allocated with same keyname then just update it. */
        ptr->address = address;
        /*
           Currently not supporting deallocation of any addresses, must be
           managed by higher calling functions. But this function should
           acknowledge that hash entry was updated.
         */
        return HASH_ENTRY_SUCCESS | HASH_ENTRY_UPDATED;
    }

    ptr = create_hash_table_elem(keyname, address);

    if (!ptr)
    {
        ERROR("Failed to allocate memory for new hash table element");
        return HASH_ENTRY_FAILED;
    }

    int result = HASH_ENTRY_SUCCESS;

    if (hashtable->table[keyhash] == NULL)
        hashtable->table[keyhash] = ptr; /*First element, hence no collision. */
    else
    {
        /* Collision detected. Must ackowledge collision occurred. */
        result = (result | HASH_ENTRY_COLLISION);
        /* Insert element to top, and update the respective strucutures. */
        ptr->next = hashtable->table[keyhash];
        hashtable->table[keyhash]->prev = ptr;
        hashtable->table[keyhash] = ptr;
    }

    hashtable->elem_count++; /* Increase element count by one */

    // Check if resize is required or not.
    uint64_t new_length = check_grow(hashtable);
    if (new_length)
    {
        /* Reallocate the hash table. */
        struct hash_table_elem **new_table =
            calloc(new_length, sizeof(struct hash_table_elem *));

        /* Also acknowledge that hash table was resized. */
        result = (result | HASH_TABLE_RESIZED);

        for (uint64_t i = 0; i < hashtable->length; i++)
        {
            /*
               Reallocate the elements to new table. Using basic linked list
               algorithms.
             */
            ptr = hashtable->table[i];

            /* Note: This may be a one chain of collided elements. */
            while (ptr)
            {
                /* Recalculate hash since length has changed. */
                uint64_t new_hash_value =
                    fnv1a_string_hash((const uint8_t *)ptr->key) % new_length;

                /* Extract one element from top. */
                struct hash_table_elem *nextPtr = ptr->next;
                hashtable->table[i] = nextPtr;

                /*
                   Point this element to new position in new table.
                   If in case already allocated due to rare collision.
                 */
                ptr->next = new_table[new_hash_value];

                if (new_table[new_hash_value])
                    new_table[new_hash_value]->prev = ptr;
                new_table[new_hash_value] = ptr;

                ptr = nextPtr;
            }
        }

        free(hashtable->table); /* Deallocate previous table structure. */
        hashtable->table = new_table;
        hashtable->length = new_length;
    }

    return result;
}

static struct hash_table_elem *get_ptr_to_elem_by_key(
    struct hash_table *hashtable,
    const char *keyname)
{
    if (!hashtable)
    {
        ERROR("Invalid hashtable object provided");
        return NULL;
    }

    if (!hashtable->table)
    {
        ERROR("Invalid hashtable object. No table allocated");
        return NULL;
    }

    if (!keyname)
    {
        ERROR("Invalid key name object");
        return NULL;
    }

    uint64_t keyhash =
        fnv1a_string_hash((const uint8_t *)keyname) % hashtable->length;

    if (hashtable->table[keyhash])  // Entry is there
    {
        struct hash_table_elem *ptr = hashtable->table[keyhash];
        while (ptr)
        {
            if (strcmp(ptr->key, keyname) == 0) return ptr;
            ptr = ptr->next;
        }
    }

    /* No need to log ERROR as the element is not present. */
    return NULL;
}

void *get_ptr_to_value_by_key(struct hash_table *hashtable, const char *keyname)
{
    struct hash_table_elem *ptr = get_ptr_to_elem_by_key(hashtable, keyname);

    if (!ptr) return NULL;

    return ptr->address;
}

static struct hash_table_elem *create_hash_table_elem(const char *key,
                                                      void *address)
{
    if (!key)
    {
        ERROR("Invalid key or object for hash table entry element");
        return NULL;
    }

    struct hash_table_elem *ptr =
        (struct hash_table_elem *)malloc(sizeof(struct hash_table_elem));
    if (!ptr)
    {
        ERROR("Failed to allocate memory for new entry");
        return NULL;
    }

    ptr->next = ptr->prev = NULL;
    ptr->key = strcpy((char *)(malloc(sizeof(char) * (strlen(key) + 1))), key);

    if (!ptr->key)
    {
        ERROR("Failed to create keyname for hash map entry");
        free(ptr);
        return NULL;
    }

    ptr->address = address;

    return ptr;
}

int delete_entry(struct hash_table *hashtable, const char *keyname)
{
    /*
       Provide a function to delete entry. Although this is handled in higher
       functions.
     */
    if (!hashtable)
    {
        INFO("Hash table already freed or received NULL.");
        return HASH_FREED;
    }

    uint64_t keyhash =
        fnv1a_string_hash((const uint8_t *)keyname) % hashtable->length;

    if (!hashtable->table[keyhash])
    {
        ERROR("Attempt to free unallocated keyhash");
        return HASH_ENTRY_FAILED;
    }

    /* If already the firsy element. */
    if (strcmp(hashtable->table[keyhash]->key, keyname) == 0)
    {
        struct hash_table_elem *ptr = hashtable->table[keyhash]->next;
        free(hashtable->table[keyhash]->key);
        free(hashtable->table[keyhash]);
        hashtable->table[keyhash] = ptr;

        if (ptr) ptr->prev = NULL;

        return HASH_ENTRY_SUCCESS;
    }
    else
    { /* Later in the list of collided elements. */
        struct hash_table_elem *prev = hashtable->table[keyhash];
        struct hash_table_elem *ptr = hashtable->table[keyhash]->next;

        while (ptr)
        {
            struct hash_table_elem *next = ptr->next;
            if (strcmp(ptr->key, keyname) == 0)
            {
                free(ptr->key);
                free(ptr);
                prev->next = next;
                if (next) next->prev = prev;

                return HASH_ENTRY_SUCCESS | HASH_ENTRY_COLLISION;
            }
            prev = ptr;
            ptr = next;
        }
    }

    ERROR("Critical error, attempt to free an item");
    return HASH_ENTRY_FAILED;
}

int free_hash_table(struct hash_table *hashtable)
{
    if (!hashtable) return HASH_FREED;

    for (size_t i = 0; i < hashtable->length; i++)
    {
        struct hash_table_elem *ptr = hashtable->table[i];
        while (ptr)
        {
            struct hash_table_elem *next = ptr->next;
            free(ptr->key);
            free(ptr);
            ptr = next;
        }
    }

    free(hashtable->table);
    free(hashtable);

    return HASH_FREED;
}
