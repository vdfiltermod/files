///////////////////////////////////////////////////////////////////////
// MPEG-2 Plugin for VirtualDub 1.8.1+
// Copyright (C) 2010-2012 fccHandler
// Copyright (C) 1998-2012 Avery Lee
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
///////////////////////////////////////////////////////////////////////

// Generic DataVector

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "DataVector.h"


DataVector::DataVector(int item_size)
	: _item_size(item_size)
{
	first = NULL;
	last  = NULL;
	count = 0;
}


DataVector::~DataVector()
{
	DataVectorBlock *i, *j;

	i = last;
	while (j = i)
	{
		i = j->prev;
		delete j;
	}
}


bool DataVector::Add(const void *pp)
{
	bool ret = false;

	if (last == NULL)
	{
		DataVectorBlock *newb = new DataVectorBlock;
		if (newb == NULL) goto Abort;

		memcpy(newb->heap, pp, _item_size);

		newb->next = NULL;
		newb->size = _item_size;

		if (first == NULL)
		{
			// New "first" block
			first       = newb;
			first->prev = NULL;
		}
		else
		{
			// New "last" block
			last        = newb;
			first->next = last;
			last->prev  = first;
		}
	}
	else
	{
		const int remains = DataVectorBlock::BLOCK_SIZE - last->size;
		int len = _item_size;

		if (len > remains)
		{
			// New "last" block
			DataVectorBlock *newb = new DataVectorBlock;
			if (newb == NULL) goto Abort;

			last->next = newb;
			newb->prev = last;
			newb->next = NULL;
			newb->size = 0;

			if (remains > 0)
			{
				memcpy(last->heap + last->size, pp, remains);
				pp = (char *)pp + remains;
				last->size += remains;
				len -= remains;
			}

			last = newb;
		}

		memcpy(last->heap + last->size, pp, len);
		last->size += len;
	}
	
	++count;
	ret = true;

Abort:
	return ret;
}


void *DataVector::MakeArray() const
{
	char *arr = NULL;

	if (count > 0)
	{
		arr = new char[count * _item_size];

		if (arr != NULL)
		{
			char *ptr = arr;

			const DataVectorBlock *dvb = first;

			while (dvb != NULL)
			{
				memcpy(ptr, dvb->heap, dvb->size);
				ptr += dvb->size;

				dvb = dvb->next;
			}
		}
	}

	return arr;
}

