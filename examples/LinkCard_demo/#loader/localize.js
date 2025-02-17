const iconv = require('iconv-lite'); // "^0.6.3"

const data = {
  "MSG_WAITING_NETWORK": "ネットワークを待っています",
  "MSG_HANDSHAKE": "ハンドシェイクを実行中",
  "MSG_REQUESTING_COMMAND": "コマンドを要求中",
  "MSG_CONFIRMING_REQUEST": "要求の確認",
  "MSG_SCAN_CARD": "カードをスキャンしてください",
  "MSG_STARTING_TRANSFER": "転送を開始中",
  "MSG_SENDING_BYTES": "バイトを送信中",
  "MSG_CARD_SENT": "カードが送信されました",
  "MSG_ERROR": "エラー",
  "MSG_PRESS_A_TRY_AGAIN": "エーを押して再試行",
  "MSG_PRESS_B_CANCEL": "ビーを押してキャンセル"
};

for (const key in data) {
  const str = data[key];
  const buf = iconv.encode(str, 'Shift_JIS');
  const hexBytes = Array.from(buf).map(byte => '0x' + byte.toString(16).padStart(2, '0'));
  hexBytes.push("0x00");
  console.log(`/* "${str}" */`);
  console.log(`const u8 ${key}[] = { ${hexBytes.join(', ')} };`);
  console.log('');
}
