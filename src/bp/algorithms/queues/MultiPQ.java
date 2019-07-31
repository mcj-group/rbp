package bp.algorithms.queues;

import javafx.util.Pair;

import java.util.Random;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Created by vaksenov on 29.07.2019.
 */
public class MultiPQ<K extends IdentifiedClass> {

    public class Node<K> extends PriorityNode<K> {
        int queue;

        public Node(K value, double priority, int queue) {
            super(value, priority);
            this.queue = queue;
        }

        public Node<K> copy() {
            return new Node<>(value, priority, queue);
        }
    }

    Heap<K>[] queues;
    Node<K>[] nodes;
    ReentrantLock[] locks;

    public MultiPQ(int maxSize, int relax) {
        nodes = new Node[maxSize];
        queues = new Heap[relax];
        for (int i = 0; i < queues.length; i++) {
            queues[i] = new Heap<>(maxSize);
        }
        locks = new ReentrantLock[relax];
        for (int i = 0; i < locks.length; i++) {
            locks[i] = new ReentrantLock();
        }
    }

    public void insert(K value, double priority) {
        int queue = ThreadLocalRandom.current().nextInt(queues.length);
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

    public void changePriority(K value, double newPriority) {
        Node<K> node = nodes[value.id];
        while (true) {
            int queue = node.queue;
            if (queue == -1) {
                return;
            }
            locks[queue].lock();
            if (node.queue != queue) {
                locks[queue].unlock();
                continue;
            }
            synchronized (node) {
                queues[queue].changePriority(node, newPriority);
            }
            locks[queue].unlock();
            return;
        }
    }

    public synchronized K extractMin() {
        while (true) {
            int i = ThreadLocalRandom.current().nextInt(queues.length);
            int j = ThreadLocalRandom.current().nextInt(queues.length);
            Node<K> vi = (Node<K>) queues[i].peek();
            Node<K> vj = (Node<K>) queues[j].peek();
            if (vi == null && vj == null) {
                continue;
            }
            Node<K> toExtract = vi == null ? vj : (vj == null ? vi :
                    vi.compareTo(vj) < 0 ? vi : vj);
            int queue = toExtract.queue;
            if (!locks[queue].tryLock()) {
                continue;
            }
            if (queues[queue].peek() != toExtract) {
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

    public Pair<K, Double> peek() {
        PriorityNode<K> peek = null;
        for (Heap queue : queues) {
            PriorityNode<K> next = queue.peek();
            if (next == null) {
                continue;
            }
            if (peek == null || peek.compareTo(next) > 0) {
                peek = next;
            }
        }
        return new Pair<>(peek.value, peek.priority);
    }

    public boolean check() {
        boolean good = true;
        for (int i = 0; i < queues.length; i++) {
            locks[i].lock();
            good &= queues[i].check();
            locks[i].unlock();
        }
        return good;
    }
}
