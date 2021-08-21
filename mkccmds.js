const fs = require('fs');

const json = fs
  .readdirSync(__dirname)
  .filter(x => x.endsWith('.cpp'))
  .map(name => ({
    directory: __dirname.replace(/\\/g, '/'),
    command: `cl.exe /nologo /DWIN32 /D_WINDOWS /W3 /EHsc /MDd /Zi /Ob0 /Od /RTC1 -std:c++latest ${name}`,
    file: `${__dirname.replace(/\\/g, '/')}/${name}`
  }))

fs.writeFileSync(
  __dirname + '/compile_commands.json',
  JSON.stringify(json),
  { encoding: 'utf-8' }
);
