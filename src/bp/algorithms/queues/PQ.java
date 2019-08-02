package bp.algorithms.queues;

/**
 * Created by vaksenov on 24.07.2019.
 */
public interface PQ<K extends IdentifiedClass> {
    public void insert(K value, double priority);
    public void changePriority(K value, double priority);
    public K extractMin();
    public PriorityNode<K> peek();
}
