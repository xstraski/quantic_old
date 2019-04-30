/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#ifndef GAME_ERROR_H
#define GAME_ERROR_H

enum error_code {
  ErrorCode_NoError = 0,
		 
  // NOTE(ivan): General IO.
  ErrorCode_NotFound,
  ErrorCode_ProtectionFault,
  ErrorCode_WrongSignature,

  // NOTE(ivan): BMP loader.
  ErrorCode_BMPLoader_CompressionNot3,
  ErrorCode_BMPLoader_BitnessNot32
};

#endif // #ifndef GAME_DRAW_H
