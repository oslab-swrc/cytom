/*
 * Copyright 2015 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "schema_generated.h"  // Already includes "flatbuffers/flatbuffers.h".
#include <flatbuffers/flatbuffers.h>

#include <vector>

using namespace MyGame;

// Example how to use FlatBuffers to create and read binary buffers.

int main(int /*argc*/, const char * /*argv*/ []) {
  // Build up a serialized buffer algorithmically:
  flatbuffers::FlatBufferBuilder builder;

  // Second, serialize the rest of the objects needed by the Monster.
  auto position = Vec3(1.0f, 2.0f, 3.0f);

  auto name = builder.CreateString("MyMonster");

  unsigned char inv_data[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  auto inventory = builder.CreateVector(inv_data, 10);

  // Shortcut for creating monster with all fields set:
  auto orc = CreateMonster(builder, &position, 150, 80, name, inventory);

  builder.Finish(orc);  // Serialize the root of the object.

  // We now have a FlatBuffer we can store on disk or send over a network.

  // ** file/network code goes here :) **
  // access builder.GetBufferPointer() for builder.GetSize() bytes
  uint32_t size = builder.GetSize();
  printf("Size is: %d\n", size);

  // Instead, we're going to access it right away (as if we just received it).

  // Get access to the root:
  auto monster = flatbuffers::GetRoot<Monster>(builder.GetBufferPointer());

  // Get and test some scalar types from the FlatBuffer.
  assert(monster->hp() == 80);
  assert(monster->mana() == 150);  // default
  assert(monster->name()->str() == "MyMonster");

  // Get and test a field of the FlatBuffer's `struct`.
  auto pos = monster->pos();
  assert(pos);
  assert(pos->z() == 3.0f);

  // Get a test an element from the `inventory` FlatBuffer's `vector`.
  auto inv = monster->inventory();
  assert(inv);
  assert(inv->Get(9) == 9);
  (void)inv;

  printf("The FlatBuffer was successfully created and verified!\n");
}
