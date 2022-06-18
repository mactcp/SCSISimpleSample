/*									StringFormat.c							*/
/*
 * StringFormat.c
 * Copyright � 1993 Apple Computer Inc. All rights reserved.
 *
 * These functions convert values to readable text.
 */
#include <Memory.h>
#include <OSUtils.h>
#pragma segment LogManager

#define NUL		'\0'

/*
 * AppendChar writes a character into the string. Note that
 * it wraps around if the string size exceeds 255 bytes.
 */
#define AppendChar(result, c) (result[++result[0]] = (c))
void						AppendUnsigned(
		StringPtr				result,
		unsigned long			value
	);
void						AppendSigned(
		StringPtr				result,
		signed long				value
	);
void						AppendUnsignedLeadingZeros(
		StringPtr				result,
		unsigned long			value,
		short					fieldWidth,
		short					terminatorChar
	);
void						AppendUnsignedInField(
		StringPtr				result,
		unsigned long			value,
		short					fieldWidth
	);
void						AppendHexLeadingZeros(
		StringPtr				result,
		unsigned long			value,
		short					fieldWidth
	);
void						AppendBytes(
		StringPtr				result,
		const Ptr				source,
		unsigned short			length
	);
void						AppendPascalString(
		StringPtr				result,
		const StringPtr			value
	);
void						AppendCString(
		StringPtr				result,
		const char				*value,
		unsigned short			maxLength
	);
void						AppendOSType(
		StringPtr				result,
		OSType					value
	);


/*
 * AppendUnsignedLeadingZeros
 * Output an n-digit decimal value with leading zeros.
 */
void
AppendUnsignedLeadingZeros(
		StringPtr			result,
		unsigned long		value,
		short				digits,
		short				terminator
		
	)
{
		if (--digits > 0)
			AppendUnsignedLeadingZeros(result, value / 10, digits, NUL);
		AppendChar(result, (value % 10) + '0');
		if (terminator != NUL)
			AppendChar(result, terminator);
}

/*
 * AppendSigned
 * Output a signed decimal longword.
 */
void
AppendSigned(
		StringPtr			result,
		signed long			value
	)
{
		if (value < 0) {
			AppendChar(result, '-');
			value = (-value);
		}
		AppendUnsigned(result, (unsigned long) value);
}

/*
 * AppendUnsigned
 * Output an unsigned decimal longword.
 */
void
AppendUnsigned(
		StringPtr			result,
		unsigned long		value
	)
{
		if (value >= 10)
			AppendUnsigned(result, value / 10);
		AppendChar(result, (value % 10) + '0');
}

/*
 * Store a number right-justified in a fixed-width field.
 */
void
AppendUnsignedInField(
		StringPtr			result,
		unsigned long		value,
		short				fieldWidth
	)
{
		Str255				work;
		
		work[0] = 0;
		AppendUnsigned(work, value);
		result[0] = 0;
		while (work[0] < fieldWidth) {
			AppendChar(result, ' ');
			--fieldWidth;
		}
		AppendPascalString(result, work);
}
		
/*
 * AppendHexLeadingZeros
 * Output a string of hex digits with leading zeros. May not
 * move memory. Note that "digits" is the field width, not
 * the number of hex bytes.
 */
void
AppendHexLeadingZeros(
		StringPtr			result,
		unsigned long		value,
		short				fieldWidth
	)
{
		if (--fieldWidth > 0)
			AppendHexLeadingZeros(result, value >> 4, fieldWidth);
		value &= 0x0F;
		AppendChar(result,
				(value < 10)
				? value + '0'
				: (value + ('A' - 10))
			);
}

/*
 * AppendOSType
 * Output a 4-byte character string. Unknown bytes (characters
 * below ' ') are replaced by '.'.
 */
void
AppendOSType(
		StringPtr				result,
		OSType					datum
	)
{
		char					value[sizeof (OSType)];
		register short			i;
		register unsigned char	c;
		
		BlockMove(&datum, value, sizeof (OSType));
		AppendChar(result, '\'');
		for (i = 0; i < sizeof (OSType); i++) {
			c = value[i];
			if (c < ' ')
				c = '.';
			AppendChar(result, c);
		}
		AppendChar(result, '\'');
}

void
AppendCString(
		StringPtr				result,
		const char				*source,
		unsigned short			maxLength
	)
{
		register unsigned char	*ptr;
		
		ptr = (unsigned char *) source;
		while (*ptr != '\0'
			&& (maxLength == 0 || (((char *) ptr) - source) < maxLength))
			AppendChar(result, *ptr++);
}

/*
 * Append a fixed-sized string.
 */
void
AppendBytes(
		StringPtr				result,
		const Ptr				source,
		unsigned short			length
	)
{
		register unsigned char	*ptr;
		
		ptr = (unsigned char *) source;
		while (length-- > 0)
			AppendChar(result, *ptr++);
}

/*
 * AppendPascalString
 */
void
AppendPascalString(
		StringPtr				result,
		const StringPtr			datum
	)
{
		register short			i;
		
		for (i = 1; i <= datum[0]; i++)
			AppendChar(result, datum[i]);
}
