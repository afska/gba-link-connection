const iconv = require('iconv-lite'); // "^0.6.3"

const data = {
  "MSG_WAITING_GAME": "ゲームを待っています",
  "MSG_SCAN_CARD": "カードをスキャンしてください",
  "MSG_TRANSFERRING": "転送中",
  "MSG_CARD_SENT": "カード送信済み",
  "MSG_ERROR": "エラー",
  "MSG_PRESS_B_CANCEL": "ビーを押してキャンセル"
};

/*
const data = {
  "MSG_WAITING_GAME":   "ＷＡＩＴＩＮＧ　ＦＯＲ　ＧＡＭＥ",
  "MSG_SCAN_CARD":      "ＰＬＥＡＳＥ　ＳＣＡＮ　ＹＯＵＲ　ＣＡＲＤ",
  "MSG_TRANSFERRING":   "ＴＲＡＮＳＦＥＲＲＩＮＧ",
  "MSG_CARD_SENT":      "ＣＡＲＤ　ＳＥＮＴ",
  "MSG_ERROR":          "ＥＲＲＯＲ",
  "MSG_PRESS_B_CANCEL":    "ＰＲＥＳＳ　Ｂ　ＴＯ　ＣＡＮＣＥＬ"
};
*/

for (const key in data) {
  const str = data[key];
  const buf = iconv.encode(str, 'Shift_JIS');
  const hexBytes = Array.from(buf).map(byte => '0x' + byte.toString(16).padStart(2, '0'));
  hexBytes.push("0x00");
  console.log(`/* "${str}" */`);
  console.log(`const u8 ${key}[] = { ${hexBytes.join(', ')} };`);
  console.log('');
}
