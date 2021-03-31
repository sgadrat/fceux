; commands to ESP

; ESP CMDS
TOESP_MSG_GET_ESP_STATUS                  = 0   ; Get ESP status
TOESP_MSG_DEBUG_GET_LEVEL                 = 1   ; Get debug level
TOESP_MSG_DEBUG_SET_LEVEL                 = 2   ; Set debug level
TOESP_MSG_DEBUG_LOG                       = 3   ; Debug / Log data
TOESP_MSG_CLEAR_BUFFERS                   = 4   ; Clear RX/TX buffers
TOESP_MSG_FROMESP_MSG_BUFFER_DROP_FROM_ESP    = 5   ; Drop messages from TX (ESP->outside world) buffer
TOESP_MSG_GET_WIFI_STATUS                 = 6   ; Get WiFi connection status
TOESP_MSG_ESP_RESTART                     = 7   ; Restart ESP

; RND CMDS
TOESP_MSG_RND_GET_BYTE                    = 8   ; Get random byte
TOESP_MSG_RND_GET_BYTE_RANGE              = 9   ; Get random byte between custom min/max
TOESP_MSG_RND_GET_WORD                    = 10  ; Get random word
TOESP_MSG_RND_GET_WORD_RANGE              = 11  ; Get random word between custom min/max

; SERVER CMDS
TOESP_MSG_SERVER_GET_STATUS               = 12  ; Get server connection status
TOESP_MSG_SERVER_PING                     = 13  ; Get ping between ESP and server
TOESP_MSG_SERVER_SET_PROTOCOL             = 14  ; Set protocol to be used to communicate (WS/UDP)
TOESP_MSG_SERVER_GET_SETTINGS             = 15  ; Get current server host name and port
TOESP_MSG_SERVER_GET_CONFIG_SETTINGS      = 16  ; Get server host name and port defined in the Rainbow config file
TOESP_MSG_SERVER_SET_SETTINGS             = 17  ; Set current server host name and port
TOESP_MSG_SERVER_RESTORE_SETTINGS         = 18  ; Restore server host name and port to values defined in the Rainbow config
TOESP_MSG_SERVER_CONNECT                  = 19  ; Connect to server
TOESP_MSG_SERVER_DISCONNECT               = 20  ; Disconnect from server
TOESP_MSG_SERVER_SEND_MESSAGE             = 21  ; Send message to server

; NETWORK CMDS
TOESP_MSG_NETWORK_SCAN                    = 22  ; Scan networks around and return count
TOESP_MSG_NETWORK_GET_SCANNED_DETAILS     = 23  ; Get scanned network details
TOESP_MSG_NETWORK_GET_REGISTERED          = 24  ; Get registered networks status
TOESP_MSG_NETWORK_GET_REGISTERED_DETAILS  = 25  ; Get registered network SSID
TOESP_MSG_NETWORK_REGISTER                = 26  ; Register network
TOESP_MSG_NETWORK_UNREGISTER              = 27  ; Unregister network

; FILE COMMANDS
TOESP_MSG_FILE_OPEN                       = 28  ; Open working file
TOESP_MSG_FILE_CLOSE                      = 29  ; Close working file
TOESP_MSG_FILE_EXISTS                     = 30  ; Check if file exists
TOESP_MSG_FILE_DELETE                     = 31  ; Delete a file
TOESP_MSG_FILE_SET_CUR                    = 32  ; Set working file cursor position a file
TOESP_MSG_FILE_READ                       = 33  ; Read working file (at specific position)
TOESP_MSG_FILE_WRITE                      = 34  ; Write working file (at specific position)
TOESP_MSG_FILE_APPEND                     = 35  ; Append data to working file
TOESP_MSG_FILE_COUNT                      = 36  ; Count files in a specific path
TOESP_MSG_FILE_GET_LIST                   = 37  ; Get list of existing files in a path
TOESP_MSG_FILE_GET_FREE_ID                = 38  ; Get an unexisting file ID in a specific path
TOESP_MSG_FILE_GET_INFO                   = 39  ; Get file info (size + crc32)

; commands from ESP

; ESP CMDS
FROMESP_MSG_READY                         = 0   ; ESP is ready
FROMESP_MSG_DEBUG_LEVEL                   = 1   ; Returns debug configuration
FROMESP_MSG_WIFI_STATUS                   = 2   ; Returns WiFi connection status

; RND CMDS
FROMESP_MSG_RND_BYTE                      = 3   ; Returns random byte value
FROMESP_MSG_RND_WORD                      = 4   ; Returns random word value

; SERVER CMDS
FROMESP_MSG_SERVER_STATUS                 = 5   ; Returns server connection status
FROMESP_MSG_SERVER_PING                   = 6   ; Returns min, max and average round-trip time and number of lost packets
FROMESP_MSG_SERVER_SETTINGS               = 7   ; Returns server settings (host name + port)
FROMESP_MSG_MESSAGE_FROM_SERVER           = 8   ; Message from server

; NETWORK CMDS
FROMESP_MSG_NETWORK_COUNT                 = 9   ; Returns number of networks found
FROMESP_MSG_NETWORK_SCANNED_DETAILS       = 10  ; Returns details for a scanned network
FROMESP_MSG_NETWORK_REGISTERED_DETAILS    = 11  ; Returns SSID for a registered network
FROMESP_MSG_NETWORK_REGISTERED            = 12  ; Returns registered networks status

; FILE CMDS
FROMESP_MSG_FILE_EXISTS                   = 13  ; Returns if file exists or not
FROMESP_MSG_FILE_DELETE                   = 14  ; Returns when trying to delete a file
FROMESP_MSG_FILE_LIST                     = 15  ; Returns path file list (FILE_GET_LIST)
FROMESP_MSG_FILE_DATA                     = 16  ; Returns file data (FILE_READ)
FROMESP_MSG_FILE_COUNT                    = 17  ; Returns file count in a specific path
FROMESP_MSG_FILE_ID                       = 18  ; Returns a free file ID (FILE_GET_FREE_ID)
FROMESP_MSG_FILE_INFO                     = 19  ; Returns file info (size + CRC32) (FILE_GET_INFO)

ESP_FILE_PATH_SAVE = 0
ESP_FILE_PATH_ROMS = 1
ESP_FILE_PATH_USER = 2

ESP_PROTOCOL_WEBSOCKET         = 0
ESP_PROTOCOL_WEBSOCKET_SECURED = 1
ESP_PROTOCOL_TCP               = 2
ESP_PROTOCOL_TCP_SECURED       = 3
ESP_PROTOCOL_UDP               = 4

rainbow_data = $5000
rainbow_flags = $5001

; Send a command to the ESP
;  tmpfield1,tmpfield2 - address of the command data
;
; Command data follows the format
;  First byte is the message length (number of bytes following this first byte).
;  Second byte is the command opcode.
;  Any remaining byte are parameters for the command.
;
; Overwrites all registers
esp_send_cmd:
.(
	ldy #0
	lda (tmpfield1), y
	sta rainbow_data

	tax
	iny
	copy_one_byte:
		lda (tmpfield1), y
		sta rainbow_data
		iny
		dex
		bne copy_one_byte

	rts
.)

; Wait for a message from the ESP
esp_wait_message:
.(
	wait_esp:
	bit $5001
	bpl wait_esp
	rts
.)
