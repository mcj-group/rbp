package bp.algorithms.queues;

import javafx.util.Pair;

import java.util.Random;
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
    }

    Heap<K>[] queues;
    Node<K>[] nodes;
    ReentrantLock[] locks;
    Random[] rnds;

    public MultiPQ(int maxSize, int relax, int threads) {
        nodes = new Node[maxSize];
        queues = new Heap[relax];
        for (int i = 0; i < queues.length; i++) {
            queues[i] = new Heap<>(maxSize);
        }
        locks = new ReentrantLock[relax];
        for (int i = 0; i < locks.length; i++) {
            locks[i] = new ReentrantLock();
        }
        rnds = new Random[threads];
        for (int i = 0; i < rnds.length; i++) {
            rnds[i] = new Random(i);
        }
    }

    public void insert(int thread, K value, double priority) {
        int queue = rnds[thread].nextInt();
        Node<K> node = nodes[value.id];
        if (node == null) {
            node = nodes[value.id] = new Node<>(value, priority, queue);
        } else {
            node.priority = priority;
            node.queue = queue;
        }
        locks[node.queue].lock();
        queues[node.queue].insert(node);
        locks[node.queue].unlock();
    }

    public void changePriority(int thread, K value, double newPriority) {
        Node<K> node = nodes[value.id];
        locks[node.queue].lock();
        queues[node.queue].changePriority(node, newPriority);
        locks[node.queue].unlock();
    }

    public synchronized K extractMin(int thread) {
        while (true) {
            int i = rnds[thread].nextInt();
            int j = rnds[thread].nextInt();
            Node<K> vi = (Node<K>) queues[i].peek();
            Node<K> vj = (Node<K>) queues[j].peek();
            Node<K> min = vi.compareTo(vj) < 0 ? vi : vj;
            if (!locks[min.queue].tryLock()) {
                continue;
            }
            PriorityNode<K> curr = queues[min.queue].extractMin();
            locks[min.queue].unlock();
            if (curr == null) {
                continue;
            }
            return curr.value;
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
}
