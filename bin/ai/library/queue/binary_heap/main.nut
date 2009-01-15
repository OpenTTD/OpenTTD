/* $Id$ */

/**
 * Binary Heap.
 *  Peek and Pop always return the current lowest value in the list.
 *  Sort is done on insertion and on deletion.
 */
class Binary_Heap
{
	_queue = null;
	_count = 0;

	constructor()
	{
		_queue = [];
	}

	/**
	 * Insert a new entry in the list.
	 *  The complexity of this operation is O(ln n).
	 * @param item The item to add to the list.
	 * @param priority The priority this item has.
	 */
	function Insert(item, priority);

	/**
	 * Pop the first entry of the list.
	 *  This is always the item with the lowest priority.
	 *  The complexity of this operation is O(ln n).
	 * @return The item of the entry with the lowest priority.
	 */
	function Pop();

	/**
	 * Peek the first entry of the list.
	 *  This is always the item with the lowest priority.
	 *  The complexity of this operation is O(1).
	 * @return The item of the entry with the lowest priority.
	 */
	function Peek();

	/**
	 * Get the amount of current items in the list.
	 *  The complexity of this operation is O(1).
	 * @return The amount of items currently in the list.
	 */
	function Count();

	/**
	 * Check if an item exists in the list.
	 *  The complexity of this operation is O(n).
	 * @param item The item to check for.
	 * @return True if the item is already in the list.
	 */
	function Exists(item);
};

function Binary_Heap::Insert(item, priority)
{
	/* Append dummy entry */
	_queue.append(0);
	_count++;

	local hole;
	/* Find the point of insertion */
	for (hole = _count - 1; hole > 0 && priority <= _queue[hole / 2][1]; hole /= 2)
		_queue[hole] = _queue[hole / 2];
	/* Insert new pair */
	_queue[hole] = [item, priority];

	return true;
}

function Binary_Heap::Pop()
{
	if (_count == 0) return null;

	local node = _queue[0];
	/* Remove the item from the list by putting the last value on top */
	_queue[0] = _queue[_count - 1];
	_queue.pop();
	_count--;
	/* Bubble down the last value to correct the tree again */
	_BubbleDown();

	return node[0];
}

function Binary_Heap::Peek()
{
	if (_count == 0) return null;

	return _queue[0][0];
}

function Binary_Heap::Count()
{
	return _count;
}

function Binary_Heap::Exists(item)
{
	/* Brute-force find the item (there is no faster way, as we don't have the priority number) */
	foreach (node in _queue) {
		if (node[0] == item) return true;
	}

	return false;
}



function Binary_Heap::_BubbleDown()
{
	if (_count == 0) return;

	local hole = 1;
	local tmp = _queue[0];

	/* Start switching parent and child until the tree is restored */
	while (hole * 2 < _count + 1) {
		local child = hole * 2;
		if (child != _count && _queue[child][1] <= _queue[child - 1][1]) child++;
		if (_queue[child - 1][1] > tmp[1]) break;

		_queue[hole - 1] = _queue[child - 1];
		hole = child;
	}
	/* The top value is now at his new place */
	_queue[hole - 1] = tmp;
}
