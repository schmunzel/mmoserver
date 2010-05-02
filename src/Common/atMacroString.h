#pragma once

#include "Utils/typedefs.h"

class atMacroString
{
public:
	atMacroString();
	~atMacroString();
	void			addTO(BString TO) {mTO = TO;}
	void			addTOId(uint64 toid) {mTOId = toid;}
	void			addTOstf(BString dir, BString str) {mTOdir = dir;mTOstr = str;}

	void			addDI(uint32 di) {mDI = di;}

	void			addTT(BString tt) {mTT = tt;}
	void			addTTId(uint64 ttid) {mTTId = ttid;}
	void			addTTstf(BString dir, BString str) {mTTdir = dir;mTTstr = str;}

	void			addTU(BString tu) {mTU = tu;}
	void			addTUId(uint64 tuid) {mTUId = tuid;}
	void			addTUstf(BString dir, BString str) {mTUdir = dir;mTUstr = str;}

	void			addMB(BString mb) {mMB = mb;}
	void			addMBstf(BString dir, BString str) {mMBdir = dir;mMBstr = str;}
	
	BString			assemble();
	void			addTextModule();

	void			setPlanetString(BString planet);
	void			addWaypoint();

	void			setWP(float x, float y, float z, BString name){mX = x; mY = y; mZ = z; mWPName = name;}


private:

	void			_addByte(uint8 mByte);
	void			_addString(BString mString);
	void			_adduint16(uint16 mWord);
	void			_adduint32(uint32 mInteger);
	void			_adduint64(uint64 mLong);
	void			_addfloat(float mFloat);


	bool	mAssembled;
	
	uint64	mTOId;
	BString	mTO;
	BString	mTOdir;
	BString	mTOstr;

	uint64	mTTId;
	BString	mTT;
	BString	mTTdir;
	BString	mTTstr;
	

	uint64	mTUId;
	BString	mTU;
	BString	mTUdir;
	BString	mTUstr;

	BString	mMB;
	BString	mMBdir;
	BString	mMBstr;

	float		mX;
	float		mY;
	float		mZ;
	uint32		mPlanetCRC;
	BString		mWPName;

	uint8 Hallo[5000];
	uint8* mPoint; 
	uint8* mPoint1;
	uint32 mSize;
	uint32 mDI;
	uint16 mCounter;
};
