/*! \file
 * \brief Memory poisoning.
 *
 * \author Max Kellermann (mk@cm4all.com)
 */

#ifndef POISON_HXX
#define POISON_HXX

#include "Poison.h"

template<typename T>
inline void
PoisonInaccessibleT(T &&t)
{
	PoisonInaccessible(&t, sizeof(t));
}

template<typename T>
inline void
PoisonUndefinedT(T &&t)
{
	PoisonUndefined(&t, sizeof(t));
}

#endif
