const fs = require('fs');

let data = "[\n"
const tiles = require('C:\\Users\\kubar\\Documents\\NC-Calculator\\data\\heatsinks.json');
for (let i = 0; i < tiles.length; i++) {
    const tile = tiles[i];
    tile.id = i;
    data += `  ${JSON.stringify(tile).replaceAll(",", ", ").replaceAll(":", ": ").replaceAll("{", "{ ").replaceAll("}", " }")},`;
    if (i === tiles.length - 1) {
        data = data.slice(0, -1); // Remove the last comma
        break
    }
    data += "\n";
}
data += "\n]";
fs.writeFileSync('C:\\Users\\kubar\\Documents\\NC-Calculator\\data\\heatsinks.json', data);
console.log(tiles);