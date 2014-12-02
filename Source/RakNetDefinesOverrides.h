/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

// USER EDITABLE FILE

#ifndef RAKNET_DEBUG_PRINTF
#define RAKNET_DEBUG_PRINTF(...) RakNet::externalLog(__FILE__, __LINE__, __VA_ARGS__)
namespace RakNet
{
	void externalLog(const char* fileName, int lineNumber, const char * format, ...);
}
#endif

