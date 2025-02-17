const iconv = require('iconv-lite'); // "^0.6.3"

function toFullWidth(str) {
  const map = {
    'A': 'Ａ',
    'B': 'Ｂ',
    '(': '（',
    ')': '）',
    '1': '１',
    '2': '２',
    '/': '／'
  };
  return str.replace(/[AB\(\)1\/2]/g, ch => map[ch] || ch);
}

const data = {
  "MSG_WAITING_GAME": "ゲームを待っています..."
};

for (const key in data) {
  const fixedStr = toFullWidth(data[key]);
  const buf = iconv.encode(fixedStr, 'Shift_JIS');
  const hexBytes = Array.from(buf).map(byte => '0x' + byte.toString(16).padStart(2, '0'));
  hexBytes.push("0x00");
  console.log(`/* "${fixedStr}" */`);
  console.log(`const u8 ${key}[] = { ${hexBytes.join(', ')} };`);
  console.log('');
}
