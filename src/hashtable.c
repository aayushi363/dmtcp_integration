#include "hashtable.h"

STRUCT_DECL(hash_table_entry)

/**
 * An entry in a hash table that uses a raw integer value as a key
 * and associates it with an arbitrary value
 */
struct hash_table_entry {
    bool valid;                /* Whether or not this hash entry is valid (only defined within bounds) */
    uint64_t key;              /* The identifier that is mapped by the hash function */
    void * value;              /* The value associated with the paired key */
};

/* A hash table entry that represents the absence of an entry */
static const hash_table_entry hash_table_invalid_entry = { .valid = false, .key = 0, .value = NULL };

/**
 * A data structure which associates data with integer key values
 * for very fast access times
 */
struct hash_table {
    size_t count;                /* The number of occupied entries in the hash table */
    size_t len;                  /* The length, in bytes, of the hash table */
    hash_table_entry *base;      /* The base address of the hash table's contents */
    hash_function hasher; /* A function to apply automatically to */
};

/**
 * Allocates and initializes a new empty hash table on the heap
 *
 * @return a reference to the new empty hash table, or NULL if
 * no more memory can be allocated by the program
 */
hash_table_ref
hash_table_create(void) {
    hash_table_ref ref = (hash_table_ref)malloc(sizeof(hash_table));

    if (ref) {
        ref->count = 0;
        ref->len = 0;
        ref->base = NULL;
    }

    return ref;
}


/**
 * Deallocates storage used to hold the contents of a hash table
 *
 * @param ref a reference to a hash table. If the reference is NULL,
 * the function does nothing
 */
void
hash_table_destroy(hash_table_ref ref) {
    if (!ref) {
        errno = EINVAL;
        return;
    }

    if (ref->base) {
        free(ref->base);
    }
    free(ref);
}

/**
 * A hash function that computes a hash value from
 * an unsigned 64-bit integer
 *
 * @param key a value to be hashed
 * @return a 64-bit hash value of the key
 */
static uint64_t
hash_key(uint64_t key) {
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = (key >> 16) ^ key;
    return key;
}

/**
 * Computes the number of possible entries that
 * could be in the hash table at its given count
 *
 * @param ref a reference to a hash table
 * @return the number of possible entries in the hash
 * table, or -1 if the hash table is -1
 */
static uint64_t
hash_table_num_slots(hash_table_ref ref) {
    return ref ? ref->len / sizeof(hash_table_entry) : UINT64_MAX;
}

/**
 * Computes the index into the given hash table's
 * dynamic storage that corresponds to the given key
 *
 * Every key corresponds to a unique index in the underlying
 * array storage of the hash table. However, since it is possible
 * for hash collisions to occur, the hashed index that corresponds
 * for any given key is *not* necessarily the same as the index
 * in the table where the data associated with the given key
 * is stored (see the `hash_table_probe()` function for more details)
 *
 * @param key a key that needs to be mapped to an index into
 * the hash table's current storage
 *
 * @return an index into the hash table that the given
 * key maps to
 */
static uint64_t
hash_table_map_key(hash_table_ref ref, uint64_t key) {
    uint64_t hash_value = hash_key(key);
    uint64_t num_entries = hash_table_num_slots(ref);
    return hash_value % num_entries;
}

static void
hash_table_rehash(hash_table_ref ref, hash_table_entry *old_base) {
    if (!ref || !ref->base) {
        errno = EINVAL;
        return;
    }

    uint64_t entries = hash_table_num_slots(ref) / 2;

    for (int i = 0; i < entries; i++) {
        hash_table_entry ent = old_base[i];

        if (ent.valid) {
            uint64_t dest = hash_table_map_key(ref, ent.key);
            ref->base[dest] = ent;
        }
    }
}

static void
hash_table_unforced_grow(hash_table_ref ref) {
    if (!ref) {
        errno = EINVAL;
        return;
    }

    if (ref->base == NULL) {
        ref->base = (hash_table_entry*) malloc(sizeof(hash_table_entry));
        ref->base[0] = hash_table_invalid_entry;
        ref->len = sizeof(hash_table_entry);
    }

    // Compute the number of bytes occupied by the entries in the table
    size_t fill = ref->count * sizeof(hash_table_entry);
    double filld = fill;

    if (filld >= (ref->len * REHASH_FACTOR)) {
        size_t old_len = ref->len;
        size_t new_len = 2 * old_len;

        // Prepare to rehash into the new memory region
        hash_table_entry *old_base = ref->base;
        hash_table_entry *new_base = (hash_table_entry*)malloc(new_len);
        bzero(new_base, new_len); /* All entries are invalid */

        // Rehash the first `rehash_size` entries in the table
        ref->len *= 2;
        ref->base = new_base;
        hash_table_rehash(ref, old_base);

        // Free the old memory
        free(old_base);
    }
}

static uint64_t
hash_table_probe_get(hash_table_ref ref, uint64_t key) {
    if (!ref) {
        errno = EINVAL;
        return UINT64_MAX;
    }

    uint64_t best_match = hash_table_map_key(ref, key);
    if (ref->count == 0)
        return best_match;

    uint64_t num_ents = hash_table_num_slots(ref);
    hash_table_entry cur;

    while ((cur = ref->base[best_match]).valid) {
        if (cur.key == key)
            return best_match;
        best_match = (best_match + 1) % num_ents;
    }

    return UINT64_MAX;
}

static uint64_t
hash_table_probe_set(hash_table_ref ref, uint64_t key, bool *replace) {
    if (!ref) {
        errno = EINVAL;
        return UINT64_MAX;
    }

    uint64_t best_match = hash_table_map_key(ref, key);
    if (ref->count == 0)
        return best_match;

    uint64_t num_ents = hash_table_num_slots(ref);
    hash_table_entry cur;

    while ((cur = ref->base[best_match]).valid) {
        if (cur.key == key) {
            *replace = true;
            return best_match;
        }
        best_match = (best_match + 1) % num_ents;
    }

    *replace = false;
    return best_match;
}

size_t
hash_table_count(hash_table_ref ref) {
    if (!ref) {
        errno = EINVAL;
        return -1;
    }

    return ref->count;
}

void*
hash_table_get(hash_table_ref ref, uint64_t key) {
    if (!ref || !ref->base) {
        errno = EINVAL;
        return NULL;
    }

    uint64_t dest;
    if ((dest = hash_table_probe_get(ref, key)) == UINT64_MAX)
        return NULL;

    return ref->base[dest].value;
}

void*
hash_table_get_implicit(hash_table_ref ref, void *key) {
    if (!ref) return NULL;
    return hash_table_get(ref, ref->hasher(key));
}

void
hash_table_set(hash_table_ref ref, uint64_t key, void *value) {
    if (!ref) {
        errno = EINVAL;
        return;
    }
    hash_table_unforced_grow(ref); /* Possibly grow the hash table */

    bool replace = false;
    uint64_t dest = hash_table_probe_set(ref, key, &replace);

    ref->base[dest] = (hash_table_entry){ .valid = true, .key = key, .value = value };
    replace ? (void)0 : ref->count++;
}

void
hash_table_set_implicit(hash_table_ref ref, void* key, void *value) {
    if (!ref) {
        errno = EINVAL;
        return;
    }
    hash_table_set(ref, ref->hasher(key), value);
}

void
hash_table_set_hash_function(hash_table_ref ref, hash_function hfunc)
{
    if (!ref) {
        errno = EINVAL;
        return;
    }
    ref->hasher = hfunc;
}

void*
hash_table_remove(hash_table_ref ref, uint64_t key) {
    if (!ref || !ref->base) {
        errno = EINVAL;
        return NULL;
    }

    uint64_t index = hash_table_map_key(ref, key);
    ref->base[index] = hash_table_invalid_entry;
    ref->count--;
}

void *
hash_table_remove_implicit(hash_table_ref ref, void* value)
{
    if (!ref || !ref->base) {
        errno = EINVAL;
        return NULL;
    }
    hash_table_remove(ref, ref->hasher(value));
}