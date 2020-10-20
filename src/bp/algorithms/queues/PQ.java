package bp.algorithms.queues;

/**
 * Created by vaksenov on 24.07.2019.
 */
public interface PQ<K extends IdentifiedClass> {
    public void insert(K value, double priority);
    default void insert(int id, K value, double priority) {
        insert(value, priority);
    }

    public void changePriority(K value, double priority);
    default void changePriority(int id, K value, double priority) {
        changePriority(value, priority);
    }

    public void changePriority(K value, double priority, double sensitivity);
    default void changePriority(int id, K value, double priority, double sensitivity) {
        changePriority(value, priority, sensitivity);
    }

    public K extractMin();
    default K extractMin(int id) {
        return extractMin();
    }

    public PriorityNode<K> peek();
}
