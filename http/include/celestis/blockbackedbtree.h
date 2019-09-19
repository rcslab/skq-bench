/**
 * @ingroup DataStructures
 *
 * @class FileBTree
 *
 * @brief BTree Implementation backed by a CVNode Block Allocator
 *
 * Roots are fixed at the allocator buffer they give them. This is so the roots
 * can easily be found by DataStructures such as the MultiBTree class.
 *
 * @author Kenneth R Hancock
 */
#ifndef __CELESTIS_BLOCK_BACKED_BTREE_H__
#define __CELESTIS_BLOCK_BACKED_BTREE_H__

#include <stdexcept>

#include <celestis/OSD/cvnodealloc.h>

extern PerfTimer STAT_NAME(BTREEI);
extern PerfTimer STAT_NAME(BTREED);
extern PerfTimer STAT_NAME(BTREEG);
extern PerfTimer STAT_NAME(BTREELEAF);
extern PerfTimer STAT_NAME(BTREEBUCKET);
extern PerfTimer STAT_NAME(BTREESINGLETON);

enum BTreeNodeType  { LEAF = 0, INNER, BUCKET, EMPTY};

struct BTreeHeader {
    blockheader alloc;
    uint32_t right;
    uint32_t parent;
    size_t n;
    BTreeNodeType type;
    size_t id()
    {
        return alloc.id;
    }
    BTreeHeader(BTreeNodeType type, uint32_t r, uint32_t parent) : 
        right(r), parent(parent), type(type)  {};
};

template<typename _K, typename _V>
class BTreeNode;

template<typename _V>
class Bucket {
    public:
        BlockAlloc * alloc;
        BTreeHeader * h;
        _V * values;
        size_t max;

        Bucket() = default;
        Bucket(size_t id, BlockAlloc * alloc) :
            alloc(alloc), max(0)
        {
            init(alloc->get(id));
        };


        Bucket(void * buffer, BlockAlloc * alloc, size_t parent) :
            alloc(alloc), max(0)
        {
            init(buffer);
            h->parent = parent;
        }

        void init(void * buffer)
        {
            h = (BTreeHeader *)buffer;
            max = (alloc->bs - sizeof(BTreeHeader)) / sizeof(_V);
            values = (_V *)(h + 1);
        }

        bool isFull() const
        {
            return h->n == max;
        }

        bool isEmpty() const
        {
            return h->n == 0;
        }

        void insert(_V value)
        {
            ASSERT(!isFull());
            values[h->n++] = value;
        }

        void remove(size_t i)
        {
            if (i != max - 1) {
                std::copy(&values[i + 1], &values[i + 1] + (h->n - i - 1), &values[i]);
            }
            h->n--;
        }
};

template<typename _K,typename _V>
class BTreeNode
{
    public:
        enum ValueType : uint8_t { EMPTY = 0, BUCKET, SINGLE};
        struct Value {
            ValueType type;
            union {
                _V value;
                uint32_t id;
            };
            Value(ValueType type, _V value) : type(type), value(value) {};
            Value(ValueType type, uint32_t id) : type(type), id(id) {};
        };

        size_t maxKeys;
        BlockAlloc * alloc;
        BTreeHeader * h;
        _K * keys;
        Value * values;
        size_t * children;

        BTreeNode() = default;
        BTreeNode(size_t id, BlockAlloc * alloc)
        {
            h =  (BTreeHeader *)alloc->get(id);
            ASSERT(h != nullptr);
            init((void *)h, alloc, h->parent);
        };

        /**
         * Constructor - The side effects of this constructor is that
         * we make sure to repair any parents that occur in this block.
         * Obviously the thing constructing this will either be the parent
         * or is the root construction.  Parents can become incorrect after the
         * root is split as the blocks under the old root will still point to this root
         * So we need to make sure these blocks are edited, but we also don't want to have
         * to read in every block under the root to repair them so this meets half way
         * and will edit blocks as their brought in. Parents are preserved so that
         * we can recursively move up the tree and split if need be. And we know these parents
         * are correct as we had to reach those children first so we know every parent is correct
         * moving upwards.
         */
        BTreeNode(void * buffer, BlockAlloc * alloc, size_t parent)
        {
            init(buffer, alloc, parent);
        };
        ~BTreeNode() {};

        void 
        init(void * buffer, BlockAlloc * alloc, size_t parent)
        {
            this->alloc = alloc;
            auto size = alloc->bs;
            h =  (BTreeHeader *)buffer;
            switch(h->type) {
                case BTreeNodeType::LEAF:
                    maxKeys = (size - sizeof(BTreeHeader)) / (sizeof(_K) + sizeof(Value));
                    keys = (_K *)((char *)h + sizeof(BTreeHeader));
                    values = (Value *)((char *)this->keys + (maxKeys * sizeof(_K)));
                    children = nullptr;
                    break;
                case BTreeNodeType::INNER:
                    maxKeys = (size - sizeof(BTreeHeader) - sizeof(size_t)) / (sizeof(_K) + sizeof(size_t));
                    keys = (_K *)((char *)h + sizeof(BTreeHeader));
                    children = (size_t *)((char *)keys + (sizeof(_K) * maxKeys));
                    values = nullptr;
                    break;
                case BTreeNodeType::BUCKET:
                case BTreeNodeType::EMPTY:
                    PANIC();
            }
            h->parent = parent;
            maxKeys--;
        }
        
        size_t 
        follow(_K &key) const
        {
            if (key < keys[0] || !h->n) {

                return 0;
            }
            auto l = 1;
            auto r = h->n - 1;
            while (l <= r) {
                size_t m = (l + r) / 2;
                if (keys[m - 1] <= key && keys[m] >= key) {
                    
                    if(keys[m] == key) return m + 1;

                    return m;
                }
                if (key > keys[m]) {
                    l = m + 1;
                } else {
                    r = m - 1;
                }
            }

            return h->n;
        }

        size_t 
        insertIndex(_K &key) {
            if ((key < keys[0]) || (h->n == 0)) {

                return 0;
            }
            ssize_t l = 0;
            ssize_t r = h->n - 1;
            while (l <= r) {
                ssize_t m = (l + r) / 2;
                if (keys[m - 1] <= key && keys[m] >= key) {
                    if (keys[m - 1] == key) return m - 1;

                    return m;
                }
                if (key > keys[m]) {
                    l = m + 1;
                } else {
                    r = m - 1;
                }
            }

            return h->n;
        }

        BTreeNode<_K, _V> 
        next(_K &key) 
        { 
            auto i = follow(key); 
            return BTreeNode<_K, _V>(alloc->get(children[i]), alloc, h->id());
        }

        ssize_t 
        search(_K &key) const
        {
            ssize_t l = 0;
            ssize_t r = h->n - 1;
            while (l <= r) {
                ssize_t m = (l + r) / 2;
                if (keys[m] == key) {

                    return m;
                }
                if (key > keys[m]) {
                    l = m + 1;
                } else {
                    r = m - 1;
                }
            }

            return -1;
        }

        /* 
         * XXX - Currently buckets are incredibly expensive.  To reduce the cost we could shard them into mini buckets. re-allocate them
         * to larger buckets once they become bigger than that.  It seems quite expensive to give a key that could be a one off 2nd key/value
         * pair its own bucket right off the bat. 
         */
        void 
        dupInsert(size_t i, _V &&value)
        {
            STAT_TSAMPLE_START(BTREEBUCKET);
            void * buff;
            Bucket<_V> b = Bucket<_V> ();
            if (values[i].type == ValueType::SINGLE) {
                // Create new Bucket
                buff = alloc->alloc();
                BTreeHeader * buff_h = new (buff) BTreeHeader(BTreeNodeType::BUCKET, 
                        0, h->id());
                b = Bucket<_V>(buff, alloc, h->id());
                b.insert(values[i].value);
                values[i] = Value(ValueType::BUCKET, buff_h->id());
            } else {
                // Get Bucket
                buff = alloc->get(values[i].value);
                b = Bucket<_V>(buff, alloc, h->parent);
            }
            b.insert(value);

            if (b.isFull()) {
                buff = alloc->alloc();
                BTreeHeader * buff_h = new (buff) BTreeHeader(BTreeNodeType::BUCKET, 
                        h->id(), h->id());
                b = Bucket<_V>(buff, alloc, h->id());
                values[i] = Value(ValueType::BUCKET, buff_h->id());
            }
            STAT_TSAMPLE_STOP(BTREEBUCKET);
        }

        void 
        mergeOrBorrow()
        {
            if (h->parent == 0) {
                if (h->n == 0 && !isLeaf()) {
                    auto child = BTreeNode<_K, _V>(children[0], alloc);
                    h->type = child.h->type;
                    init((void *)h, alloc, 0);
                    std::copy(&child.keys[0], &child.keys[0] + child.h->n, &keys[0]);
                    if (isLeaf()) {
                        std::copy(&child.values[0], &child.values[0] + child.h->n, &values[0]);
                    } else {
                        std::copy(&child.children[0], &child.children[0] + (child.h->n + 1), &children[0]);
                    }
                    h->n = child.h->n;
                    alloc->free((void *)child.h);
                }

                return;
            }

            auto parent = BTreeNode<_K, _V>(h->parent, alloc);
            if (h->right == 0) {
                // No more right borrowing
                if (h->n == 0) {
                    // Its empty, so pop off the last element of the parent. This
                    // case only happens with the RIGHT most leaf
                    parent.h->n --;
                    // Tell the left sibling that there is no more right.
                    auto left = BTreeNode<_K, _V>(parent.children[h->n], alloc);
                    left.h->right = 0;
                    alloc->free((void *)h);
                }
                    
                return;
            }
            auto right = BTreeNode<_K, _V>(h->right, alloc);
            auto i = parent.follow(keys[0]);
            if (right.h->n <= (maxKeys / 2)) {
                // Merge this bad boi
                if (isLeaf()) {
                    std::copy(&right.keys[0], &right.keys[0] + (right.h->n), &keys[h->n]);
                    std::copy(&right.values[0], &right.values[0] + (right.h->n), &values[h->n]);
                } else {
                    keys[h->n] = parent.keys[i];
                    h->n++;
                    std::copy(&right.keys[0], &right.keys[0] + (right.h->n), &keys[h->n]);
                    std::copy(&right.children[0], &right.children[0] + (right.h->n + 1), &children[h->n]);
                }
                h->n += right.h->n;
                parent.removeKey(parent.keys[i]);
                h->right = right.h->right;
                alloc->free((void *)right.h);
            } else {
                // Borrow
                if (isLeaf()) {
                    auto key = right.keys[0];
                    auto val = right.values[0];
                    right.shift(1, -1);
                    insertAt(key, std::forward<Value>(val), h->n);
                    parent.keys[i] = right.keys[0];
                } else {
                    auto child = right.children[0];
                    auto key =  right.keys[0];
                    std::copy(&right.keys[1], &right.keys[1] + (right.h->n - 1), &right.keys[0]);
                    std::copy(&right.children[1], &right.children[1] + (right.h->n), &right.children[0]);
                    ASSERT(right.h->n > 0);
                    insertKeyChild(parent.keys[i], child);
                    parent.keys[i] = key;
                }
                right.h->n--;
            }
        }

        int
        remove(_K key)
        {
            if (isLeaf()) {
                removeKey(key); 
            } else {
                auto n = next(key);
                n.remove(key);
            }
            if (h->n < (maxKeys / 2)) {
                mergeOrBorrow();
            }

            return 0;
        }

        void
        shift(size_t at, int s)
        {
            std::copy(&keys[at], &keys[at] + (h->n - at), &keys[at + s]);
            if (isLeaf()) {
                std::copy(&values[at], &values[at] + (h->n - at), &values[at + s]);
            } else {
                std::copy(&children[at + 1], &children[at + 1] + (h->n - at + 1), &children[at + s + 1]);
            }
        }

        int
        insertAt(_K &key, Value &&val, int i = -1)
        {
            
            STAT_TSAMPLE_START(BTREESINGLETON);
            ASSERT(isLeaf())
            if (i < h->n) {
                shift(i, 1);
            }
            keys[i] = key;
            values[i] = val;
            h->n++;
            STAT_TSAMPLE_STOP(BTREESINGLETON);

            return 0;
        }

        int
        insert(_K &key, _V &value)
        {
            if (isLeaf()) {
                STAT_TSAMPLE_START(BTREELEAF);
                auto i = insertIndex(key);
                // BUCKET INSERT
                if (keys[i] == key && values[i].type != ValueType::EMPTY) {
                    dupInsert(i, std::forward<_V>(value)); 
                } else {
                    insertAt(key, Value(ValueType::SINGLE, value), i);
                }
                STAT_TSAMPLE_STOP(BTREELEAF);
            } else {
                auto n = next(key);
                n.insert(key, value);
            }
            if (isFull()) {
                split();
            }

            return 0;
        }

        void removeKey(_K key) {
            auto i = search(key);
            if (i != h->n - 1) {
                shift(i + 1, -1);
            }
            h->n--;
        }

        void
        insertKeyChild(_K key, size_t child) 
        {
            auto i = insertIndex(key);
            if (i < h->n) {
                shift(i, 1);
            }
            keys[i] = key;
            children[i + 1] = child;
            h->n++;
        }

        void 
        split()
        {
            if (h->parent == 0) {
                auto buff = alloc->alloc();
                auto id = ((BTreeHeader *)buff)->id();
                memcpy(buff, h, alloc->bs);
                auto l = BTreeNode<_K, _V>(buff, alloc, h->id());
                l.h->alloc.id = id;
                id = h->id();
                bzero(h, alloc->bs);
                h->alloc.id = id;
                h->type = BTreeNodeType::INNER;
                init((void *)h, alloc, 0);
                children[0] = l.h->id();
                l.split();
            } else {
                auto header = new (alloc->alloc()) BTreeHeader(h->type, h->right, h->parent);
                h->right = header->id();
                auto right = BTreeNode<_K, _V>((void *)header, alloc, h->parent);
                size_t mid = h->n / 2;
                auto parent = BTreeNode<_K, _V>(h->parent, alloc);
                if (isLeaf()) {
                    std::copy(&keys[mid], &keys[mid] + (h->n - mid), right.keys);
                    std::copy(&values[mid], &values[mid] + (h->n - mid), right.values);
                    right.h->n = h->n - mid;
                    h->n = mid;
                    parent.insertKeyChild(right.keys[0], right.h->id());
                } else {
                    auto key = keys[mid];
                    std::copy(&keys[mid + 1], &keys[mid + 1] + (h->n - mid - 1), right.keys);
                    std::copy(&children[mid + 1], &children[mid + 1] + (h->n - mid), right.children);
                    right.h->n = h->n - mid - 1;
                    h->n = mid;
                    parent.insertKeyChild(key, right.h->id());
                }
                ASSERT(right.h->n > 0);
            }
        }

        bool 
        isNull() const
        {
            return h == nullptr;
        }

        bool 
        isLeaf() const
        {
            return h->type == BTreeNodeType::LEAF;
        }

        bool 
        isFull() const
        {
            return h->n == maxKeys;
        }

        void 
        clear()
        {
            h->n = 0;
        }

};

template<typename _K,typename _V>
class BTreeRef
{
    public:
        ssize_t index;
        BTreeNodeType type;
        BTreeNode<_K, _V> node;
        struct {
            _K key;
            Bucket<_V> vals;
        } bucket;

        BTreeRef() : index(-1), type(BTreeNodeType::EMPTY) {};
        BTreeRef(const BTreeRef<_K, _V> &other) = default;
        ~BTreeRef() {};

        BTreeRef(BTreeNode<_K, _V> &n, size_t index) :
            index(index)
        {
            if (n.values[index].type != BTreeNode<_K, _V>::ValueType::EMPTY) {
                type = n.isLeaf() ? BTreeNodeType::LEAF : BTreeNodeType::INNER;
                node = n;
            } else {
                type = BTreeNodeType::EMPTY;
            }
        };

        BTreeRef(Bucket<_V> &b, _K &key, size_t index) :
            index(index), type(BTreeNodeType::BUCKET) 
        {
           bucket.key = key;
           bucket.vals = b;
        };

        bool found() const
        {
            return index != -1;
        }

        BTreeRef<_K, _V> next()
        {
            BTreeRef<_K, _V> n = BTreeRef<_K, _V>(*this);
            switch (type) {
                case BTreeNodeType::LEAF:
                    // Last element in this node;
                    if ((index == node.maxKeys - 1) || (index == node.h->n - 1)) {
                        if (node.h->right != 0) {
                            // Get next node to the right
                            auto c = BTreeNode<_K, _V>(node.h->right, node.alloc);
                            n = BTreeRef<_K, _V>(c, 0);
                        } else {
                            // No more elements;
                            n = BTreeRef<_K, _V>();
                        }
                    } else {
                        n.index = this->index + 1;
                    }
                    break;
                case BTreeNodeType::BUCKET:
                // Last value in this bucket
                    if((index == bucket.vals.max - 1) || (index == bucket.vals.h->n - 1)) {
                        if (bucket.vals.h->right != 0) {
                            // Last value for this key, get next bucket.
                            auto b = Bucket<_V>(bucket.vals.h->right, bucket.vals.alloc);
                            n = BTreeRef<_K, _V>(b, bucket.key, 0);
                        } else {
                            // No more buckets, get next index in the parent node.
                            // Get the bucket back then just re-use the next functionality;
                            auto bp = BTreeNode<_K, _V>(bucket.vals.h->parent, bucket.vals.alloc);
                            n = BTreeRef<_K, _V>(bp, bp.search(bucket.key)).next();
                        }
                    } else {
                        n.index = this->index + 1;
                    }
                    break;
                // We can't ask for a next on a INNER or EMPTY node.
                case BTreeNodeType::EMPTY:
                case BTreeNodeType::INNER:
                    PANIC();

            }

            return n;
        }

        _K key() const
        {
            switch(type) {
                case BTreeNodeType::LEAF:
                    return node.keys[index];

                case BTreeNodeType::BUCKET:
                    return bucket.key; 
                case BTreeNodeType::EMPTY:
                case BTreeNodeType::INNER:
                    PANIC();
            }
        }

        _V val() const
        {
            switch(type) {
                case BTreeNodeType::LEAF:
                    return node.values[index].value;
                case BTreeNodeType::BUCKET:
                    return bucket.vals.values[index]; 
                case BTreeNodeType::EMPTY:
                case BTreeNodeType::INNER:
                    PANIC();
            }
        }
};

template<typename _K, typename _V>
class BlockBackedBTree
{
    public:
        BlockAlloc * alloc;
        BTreeNode<_K, _V> root;

        BlockBackedBTree(BlockAlloc * alloc, void * root_buff) : alloc(alloc)        
        {
            root = BTreeNode<_K, _V>(root_buff, alloc, 0);
        }


        BTreeRef<_K, _V> get(_K key) const
        {
            STAT_TSAMPLE_START(BTREEG);
            auto current = search(key);
            auto i = current.search(key);
            STAT_TSAMPLE_STOP(BTREEG);

            return wrap(current, i);
        }

        BTreeRef<_K, _V> getClosest(_K key) const
        {
            STAT_TSAMPLE_START(BTREEG);
            auto current = search(key);
            auto i = current.follow(key);
            STAT_TSAMPLE_STOP(BTREEG);

            return wrap(current, i);
        }

        int insert(_K key, _V value)
        {
            STAT_TSAMPLE_START(BTREEI);
            auto ref = root.insert(key, value);
            STAT_TSAMPLE_STOP(BTREEI);

            return ref;
        }

        int remove(BTreeRef<_K, _V> &ref)
        {
            STAT_TSAMPLE_START(BTREED);
            ASSERT(ref.type == BTreeNodeType::LEAF || ref.type == BTreeNodeType::BUCKET);
            if (ref.type == BTreeNodeType::LEAF) {
                // We start at root so we can recursively merge if need be.
                root.remove(ref.key());
            } else {
                // Bucket removal
                ref.bucket.vals.remove(ref.index);
                /*  If the bucket is empty we need to remove the bucket if its the last element in
                 *  its bucket. Or remove the key and value from the overall node if its the last element
                 *  Currently I keep the bucket available because Its optimistic that if the user
                 *  used the value as a duplicate once then they are more likely to do it again. 
                 *  */
                if (ref.bucket.vals.isEmpty()) {
                    auto parent = BTreeNode<_K, _V>(ref.bucket.vals.h->parent, alloc);
                    auto i = parent.search(ref.bucket.key);
                    // Currently have to use if instead of ASSERT, as template comma screws up the
                    // ASSERT macro
                    ASSERT((parent.values[i].type == BTreeNode<_K, _V>::ValueType::BUCKET));
                    auto past = Bucket<_V>(parent.values[i].value, alloc);
                    if (past.h->id() == ref.bucket.vals.h->id()) {
                        parent.remove(ref.bucket.key);
                    } else {
                        // Last element in this specific bucket;
                        auto current = past;
                        while(current.h->id() != ref.bucket.vals.h->id()) {
                            past = current;
                            ASSERT(current.h->right != 0);
                            current = Bucket<_V>(current.h->right, alloc);
                        }
                        past.h->right = current.h->right;
                    }
                    // Free the bucket
                    alloc->free((void *)ref.bucket.vals.h);
                }             
            }
            STAT_TSAMPLE_STOP(BTREED);

            return 0;
        }

        std::string
        toString()
        {
            return "";
        }

    private:
        BTreeRef<_K, _V> wrap(BTreeNode<_K, _V> &current, ssize_t i) const
        {
            if (i >= 0 && i < current.h->n) {
                if (current.values[i].type == BTreeNode<_K, _V>::ValueType::BUCKET) {
                    auto buff = alloc->get(current.values[i].value);
                    auto b = Bucket<_V>(buff, alloc, current.h->id());

                    return BTreeRef<_K, _V>(b, current.keys[i], 0);
                }

                return BTreeRef<_K, _V>(current, i);
            }
            return BTreeRef<_K, _V>();
        }

        BTreeNode<_K, _V> search(_K key) const
        {
            BTreeNode<_K, _V> current = root;

            while(!current.isLeaf()) {
                current = current.next(key);
            }
            return current;
        }

};
#endif
