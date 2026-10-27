// Table with the definition of every expression.
#ifndef FACE_EXPRESSIONS_H
#define FACE_EXPRESSIONS_H

#include "FaceTypes.h"

namespace Expressions {

const ExpressionDef &get(uint8_t id);
const char *name(uint8_t id);
const char *moodName(uint8_t mood);
// Returns EX_NONE when the name is unknown (case insensitive).
uint8_t find(const char *name);

}  // namespace Expressions

#endif
