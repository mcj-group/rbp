package bp.algorithms.queues;

import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by vaksenov on 29.07.2019.
 */
public class RandomPQ<K extends IdentifiedClass> extends MultiPQ<K> {

    public RandomPQ(int maxSize, int p) {
        super(maxSize, p);
    }

    public void insert(int pid, K value, double priority) {
        int queue = pid == -1 ? ThreadLocalRandom.current().nextInt(queues.length) :
                pid;
        Node<K> node = nodes[value.id];
        if (node == null) {
            node = nodes[value.id] = new Node<>(value, priority, queue);
        } else {
            node.priority = priority;
        }
        locks[queue].lock();
        synchronized (node) {
            node.queue = queue;
            queues[node.queue].insert(node);
        }
        locks[node.queue].unlock();
    }

    public K extractMin(int pid) {
        while (true) {
            Node<K> toExtract = (Node<K>) queues[pid].realPeek();
            if (toExtract == null) {
                continue;
            }
            int queue = toExtract.queue;
            if (toExtract.queue == -1) {
                continue;
            }

            if (!locks[queue].tryLock()) {
                continue;
            }
            if (queues[queue].realPeek() != toExtract) {
                locks[queue].unlock();
                continue;
            }

            synchronized (toExtract) {
                queues[queue].extractMin();
                toExtract.queue = -1;
            }
            locks[queue].unlock();

            return toExtract.value;
        }
    }

    public K extractMin() {
        return extractMin(ThreadLocalRandom.current().nextInt(queues.length));
    }

    public PriorityNode<K> peek() {
        PriorityNode<K> peek = null;
        for (int i = 0; i < queues.length; i++) {
            Heap<K> queue = queues[i];
            PriorityNode<K> next = queue.peek();
            if (next == null) {
                continue;
            }
            next = next.copy();
            if (peek == null || peek.compareTo(next) > 0) {
                peek = next;
            }
        }
        return peek;
    }

}
