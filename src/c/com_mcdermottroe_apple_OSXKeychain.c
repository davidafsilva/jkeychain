/*
 * Copyright (c) 2011, Conor McDermottroe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <Security/Security.h>

#include "com_mcdermottroe_apple_OSXKeychain.h"

#define throw_OSXKeychainException(message) throw_exception(env, "com/mcdermottroe/apple/OSXKeychainException", message)

/* A simplified structure for dealing with jstring objects. Use jstring_unpack
 * and jstring_unpacked_free to manage these.
 */
typedef struct {
	int len;
	char* str;
} jstring_unpacked;

/* Throw an exception.
 *
 * Parameters:
 *	env				The JNI environment.
 *	exceptionClass	The name of the exception class.
 *	message			The message to pass to the Exception.
 */
void throw_exception(JNIEnv* env, const char* exceptionClass, const char* message) {
	jclass cls = (*env)->FindClass(env, exceptionClass);
	(*env)->ThrowNew(env, cls, message);
}

/* Unpack the data from a jstring and put it in a jstring_unpacked.
 *
 * Parameters:
 *	env	The JNI environment.
 *	js	The jstring to unpack.
 *	ret	The jstring_unpacked in which to store the result.
 */
void jstring_unpack(JNIEnv* env, jstring js, jstring_unpacked* ret) {
	if (ret == NULL) {
		return;
	}
	if (env == NULL || js == NULL) {
		ret->len = 0;
		ret->str = NULL;
		return;
	}

	/* Get the length of the string. */
	ret->len = (int)((*env)->GetStringLength(env, js));
	if (ret->len <= 0) {
		ret->len = 0;
		ret->str = NULL;
		return;
	}

	/* Allocate enough space for the string. */
	ret->str = (char*)calloc(ret->len + 1, sizeof(char));
	if (ret->str == NULL) {
		ret->len = 0;
		throw_OSXKeychainException("Failed to allocate memory to unpack a jstring.");
		return;
	}

	/* Copy the string into the structure. */
	(*env)->GetStringUTFRegion(env, js, 0, ret->len, ret->str);
}

/* Clean up a jstring_unpacked after it's no longer needed.
 *
 * Parameters:
 *	jsu	A jstring_unpacked structure to clean up.
 */
void jstring_unpacked_free(jstring_unpacked* jsu) {
	if (jsu != NULL && jsu->str != NULL) {
		free(jsu->str);
		jsu->len = 0;
		jsu->str = NULL;
	}
}

/* Implementation of OSXKeychain.findInternetPassword(). See the Java docs for
 * explanations of the parameters.
 */
JNIEXPORT jstring JNICALL Java_com_mcdermottroe_apple_OSXKeychain__1findInternetPassword(JNIEnv* env, jobject obj, jstring serverName, jstring securityDomain, jstring accountName, jstring path, jint port) {
	OSStatus status;
	jstring_unpacked server_name;
	jstring_unpacked security_domain;
	jstring_unpacked account_name;
	jstring_unpacked server_path;
	jstring result;

	/* This is the password buffer which will be used by
	 * SecKeychainFindInternetPassword
	 */
	void* password;
	UInt32 password_length;

	/* Unpack all the jstrings into useful structures. */
	jstring_unpack(env, serverName, &server_name);
	jstring_unpack(env, securityDomain, &security_domain);
	jstring_unpack(env, accountName, &account_name);
	jstring_unpack(env, path, &server_path);

	/* Query the keychain */
	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainUser);
	if (status != errSecSuccess) {
		throw_OSXKeychainException("Failed to set preference domain");
		return NULL;
	}
	status = SecKeychainFindInternetPassword(
		NULL,
		server_name.len,
		server_name.str,
		security_domain.len,
		security_domain.str,
		account_name.len,
		account_name.str,
		server_path.len,
		server_path.str,
		port,
		kSecProtocolTypeAny,
		kSecAuthenticationTypeAny,
		&password_length,
		&password,
		NULL
	);
	if (status != errSecSuccess) {
		throw_OSXKeychainException("SecKeychainFindInternetPassword failed");
	}
	((char*)password)[password_length] = 0;

	/* Create the return value. */
	result = (*env)->NewStringUTF(env, password);

	/* Clean up. */
	SecKeychainItemFreeContent(NULL, password);
	jstring_unpacked_free(&server_name);
	jstring_unpacked_free(&security_domain);
	jstring_unpacked_free(&account_name);
	jstring_unpacked_free(&server_path);

	return result;
}