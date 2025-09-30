#ifndef _DATAVECTOR_H_
#define _DATAVECTOR_H_

class DataVector {

private:
	struct DataVectorBlock
	{
		enum { BLOCK_SIZE = 4096 - ((sizeof(void*) * 2) + sizeof(int)) };

		DataVectorBlock* prev;
		DataVectorBlock* next;
		int              size;
		char             heap[BLOCK_SIZE];
	};

	DataVectorBlock* first;
	DataVectorBlock* last;
	const int        _item_size;
	unsigned long    count;

public:
	DataVector(int item_size);
	~DataVector();

	bool Add(const void *pp);
	void *MakeArray() const;

	unsigned long Length() const { return count; };
};

#endif	// _DATAVECTOR_H_
