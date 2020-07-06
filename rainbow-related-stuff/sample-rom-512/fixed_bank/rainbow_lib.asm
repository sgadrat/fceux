TOESP_MSG_GET_ESP_STATUS = 0              ; Get ESP status
TOESP_MSG_DEBUG_GET_LEVEL = 1             ; Get debug level
TOESP_MSG_DEBUG_SET_LEVEL = 2             ; Set debug level
TOESP_MSG_DEBUG_LOG = 3                   ; Debug / Log data
TOESP_MSG_CLEAR_BUFFERS = 4               ; Clear RX/TX buffers
TOESP_MSG_E2N_BUFFER_DROP = 5             ; Drop messages from TX (ESP->NES) buffer
TOESP_MSG_GET_WIFI_STATUS = 6             ; Get WiFi connection status
TOESP_MSG_GET_RND_BYTE = 7                ; Get random byte
TOESP_MSG_GET_RND_BYTE_RANGE = 8          ; Get random byte between custom min/max
TOESP_MSG_GET_RND_WORD = 9                ; Get random word
TOESP_MSG_GET_RND_WORD_RANGE = 10         ; Get random word between custom min/max
TOESP_MSG_GET_SERVER_STATUS = 11          ; Get server connection status
TOESP_MSG_GET_SERVER_PING = 12            ; Get ping between ESP and server
TOESP_MSG_SET_SERVER_PROTOCOL = 13        ; Set protocol to be used to communicate (WS/UDP)
TOESP_MSG_GET_SERVER_SETTINGS = 14        ; Get current server host name and port
TOESP_MSG_GET_SERVER_CONFIG_SETTINGS = 15 ; Get server host name and port defined in the Rainbow config file
TOESP_MSG_SET_SERVER_SETTINGS = 16        ; Set current server host name and port
TOESP_MSG_RESTORE_SERVER_SETTINGS = 17    ; Restore server host name and port to values defined in the Rainbow config
TOESP_MSG_CONNECT_TO_SERVER = 18          ; Connect to server
TOESP_MSG_DISCONNECT_FROM_SERVER = 19     ; Disconnect from server
TOESP_MSG_SEND_MESSAGE_TO_SERVER = 20     ; Send message to rainbow server
TOESP_MSG_FILE_OPEN = 21                  ; Open working file
TOESP_MSG_FILE_CLOSE = 22                 ; Close working file
TOESP_MSG_FILE_EXISTS = 23                ; Check if file exists
TOESP_MSG_FILE_DELETE = 24                ; Delete a file
TOESP_MSG_FILE_SET_CUR = 25               ; Set working file cursor position a file
TOESP_MSG_FILE_READ = 26                  ; Read working file (at specific position)
TOESP_MSG_FILE_WRITE = 27                 ; Write working file (at specific position)
TOESP_MSG_FILE_APPEND = 28                ; Append data to working file
TOESP_MSG_FILE_COUNT = 29                 ; Count files in a specific path
TOESP_MSG_FILE_GET_LIST = 30              ; Get list of existing files in a path
TOESP_MSG_FILE_GET_FREE_ID = 31           ; Get an unexisting file ID in a specific path
TOESP_MSG_FILE_GET_INFO = 32              ; Get file info (size + crc32)

FROMESP_MSG_READY = 0                     ; ESP is ready
FROMESP_MSG_DEBUG_LEVEL = 1               ; Returns debug configuration
FROMESP_MSG_FILE_EXISTS = 2               ; Returns if file exists or not
FROMESP_MSG_FILE_DELETE = 3               ; Returns when trying to delete a file
FROMESP_MSG_FILE_LIST = 4                 ; Returns path file list (FILE_GET_LIST)
FROMESP_MSG_FILE_DATA = 5                 ; Returns file data (FILE_READ)
FROMESP_MSG_FILE_COUNT = 6                ; Returns file count in a specific path
FROMESP_MSG_FILE_ID = 7                   ; Returns a free file ID (FILE_GET_FREE_ID)
FROMESP_MSG_FILE_INFO = 8                 ; Returns file info (size + CRC32) (FILE_GET_INFO)
FROMESP_MSG_WIFI_STATUS = 9               ; Returns WiFi connection status
FROMESP_MSG_SERVER_STATUS = 10            ; Returns server connection status
FROMESP_MSG_SERVER_PING = 11              ; Returns min, max and average round-trip time and number of lost packets
FROMESP_MSG_SERVER_SETTINGS = 12          ; Returns server settings (host name + port)
FROMESP_MSG_RND_BYTE = 13                 ; Returns random byte value
FROMESP_MSG_RND_WORD = 14                 ; Returns random word value
FROMESP_MSG_MESSAGE_FROM_SERVER = 15      ; Message from server

ESP_FILE_PATH_SAVE = 0
ESP_FILE_PATH_ROMS = 1
ESP_FILE_PATH_USER = 2

ESP_PROTOCOL_WEBSOCKET = 0
ESP_PROTOCOL_UDP = 1

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
