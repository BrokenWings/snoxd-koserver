#include "StdAfx.h"

void CUser::LetterSystem(Packet & pkt)
{
	uint8 opcode = pkt.read<uint8>();
		
	switch (opcode)
	{
	case LETTER_UNREAD:
		TRACE("LETTER_UNREAD\n");
		break;

	case LETTER_LIST:
		TRACE("LETTER_LIST\n");
		break;

	case LETTER_HISTORY:
		TRACE("LETTER_HISTORY\n");
		break;

	case LETTER_GET_ITEM:
		TRACE("LETTER_GET_ITEM\n");
		break;

	case LETTER_READ:
		TRACE("LETTER_READ\n");
		break;

	case LETTER_DELETE:
		TRACE("LETTER_DELETE\n");
		break;

	case LETTER_ITEM_CHECK: // not sure what this is
		TRACE("LETTER_ITEM_CHECK\n");
		break;

	default:
		TRACE("Unknown letter packet: %X\n", opcode);
	}
}
