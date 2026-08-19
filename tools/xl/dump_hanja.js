const fs=require('fs');
const H=require('../../ime/core/hanja.js');
const qs=fs.readFileSync('/tmp/queries.txt','utf8').split('\n').filter(Boolean);
fs.writeFileSync('/tmp/hj_js.txt', qs.map(q=>q+'\t'+H.lookup(q).join(' ')).join('\n')+'\n');
