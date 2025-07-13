const fs = require('fs');

const rates = [
    60, 90, 140, 120, 160, 185, 90, 120, 130, 120, 150, 80, 160, 80, 120, 110, 150, 40,
    135, 115, 40, 175, 180, 160, 170, 160, 70, 60, 95, 145, 130, null, null
];

let data = "[\n"
const tiles = require('C:\\Users\\kubar\\Documents\\NC-Calculator\\data\\components.json');
for (let i = 0; i < tiles.length; i++) {
    const tile = tiles[i];
    tile.active = false;
    tile.cooling_rate = rates[i];
    data += `  ${JSON.stringify(tile).replaceAll(",", ", ")},`;
    if (i === tiles.length - 1) {
        data = data.slice(0, -1); // Remove the last comma
        break
    }
    data += "\n";
}
data += "\n]";
fs.writeFileSync('C:\\Users\\kubar\\Documents\\NC-Calculator\\data\\components.json', data);
console.log(tiles);