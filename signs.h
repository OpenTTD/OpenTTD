#ifndef SIGNS_H
#define SIGNS_H

typedef struct SignStruct {
	StringID     str;
	ViewportSign sign;
	int32        x;
	int32        y;
	byte         z;
	byte owner; // placed by this player. Anyone can delete them though.
							// OWNER_NONE for gray signs from old games.

	uint16       index;
} SignStruct;

VARDEF SignStruct _sign_list[40];
VARDEF uint _sign_size;

static inline SignStruct *GetSign(uint index)
{
	assert(index < _sign_size);
	return &_sign_list[index];
}

#define FOR_ALL_SIGNS(s) for(s = _sign_list; s != &_sign_list[_sign_size]; s++)

VARDEF SignStruct *_new_sign_struct;

void UpdateAllSignVirtCoords(void);
void PlaceProc_Sign(uint tile);

/* misc.c */
void ShowRenameSignWindow(SignStruct *ss);

#endif /* SIGNS_H */
