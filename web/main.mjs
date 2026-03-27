/** @typedef Component
 * @property {string} title
 * @property {string} name
 * @property {string} className
 * @property {number} cooling_rate
 */

/** @type {Component[]} */
import COMPONENTS from "./components.json" with {type: "json"};
import "https://cdn.jsdelivr.net/npm/chart.js@2.9.3/dist/Chart.min.js"
import "https://code.jquery.com/jquery-3.5.0.slim.min.js"
import "./FissionOpt.js";
import {Int16, Int32, write} from "https://cdn.jsdelivr.net/npm/nbtify@2.2.0/+esm";

const CELL_ID = COMPONENTS.length - 3;
const MODERATOR_ID = COMPONENTS.length - 2;
const AIR_ID = COMPONENTS.length - 1;
const N_HEATSINKS = CELL_ID;

function createBlockTypeTable() {
  const table = $('<table></table>');
  [
    { id: "blockType", label: 'Block Type', getValue: bt => `<th title="${bt.title}" class="${bt.className}">${bt.name}</th>` },
    { id: "rate", label: 'Cooling Rate (H/t)', getValue: bt => bt.cooling_rate ? `<td><input value="${bt.cooling_rate}"></td>` : `<td>&mdash;</td>` },
    { id: "limit", label: 'Max Allowed', getValue: () => '<td><input type="text"></td>' }
  ].forEach(rowDef => {
    const row = $(`<tr id="${rowDef.id}"></tr>`);
    row.append(`<th>${rowDef.label}</th>`);
    COMPONENTS.forEach(bt => row.append(rowDef.getValue(bt)))
    table.append(row);
  });
  $('.tables').append(table);
}

function displayTile(tile) {
  const component = COMPONENTS[tile] || COMPONENTS[AIR_ID];
  const result = $('<span>' + component.name + '</span>').addClass(component.className);
  result.attr('title', component.title);
  return result;
}

function saveTile(tile) {
  const component = COMPONENTS[tile] || COMPONENTS[AIR_ID];
  return component.saveName;
}

$(() => { FissionOpt().then((FissionOpt) => {
  createBlockTypeTable()
  const run = $('#run'), pause = $('#pause'), stop = $('#stop'), design = $('#design');
  const save = $('#save'), bgString = $('#bgString'), progress = $('#progress');
  const fuelBasePower = $('#fuelBasePower'), fuelBaseHeat = $('#fuelBaseHeat');
  const settings = new FissionOpt.FissionSettings();
  let lossElement, lossPlot, opt = null, timeout = null;

  const fuelPresets = {
    TBU: [4800, 27],
    LEU235: [12960, 50],
    LEU233: [17280, 60],
    HEU235: [51840, 150],
    HEU233: [69120, 180],
    LEN236: [7200, 36],
    HEN236: [28800, 108],
    LEP239: [9600, 40],
    LEP241: [23520, 70],
    HEP239: [38400, 210],
    HEP241: [94080, 120],
    LEA242: [26880, 94],
    HEA242: [107520, 282]
  };
  for (const [name, [power, heat]] of Object.entries(fuelPresets)) {
    $('#' + name).click(() => {
      if (opt !== null)
        return;
      fuelBasePower.val(power);
      fuelBaseHeat.val(heat);
    });
  }

  const rates = []
  const limits = []
  $('#rate input').each(function() { rates.push($(this)); });
  $('#limit input').each(function() { limits.push($(this)); });


  function updateDisables() {
    $('#settings input').prop('disabled', opt !== null);
    $('#settings a')[opt === null ? 'removeClass' : 'addClass']('disabledLink');
    run[timeout === null ? 'removeClass' : 'addClass']('disabledLink');
    pause[timeout !== null ? 'removeClass' : 'addClass']('disabledLink');
    stop[opt !== null ? 'removeClass' : 'addClass']('disabledLink');
  }

  function displaySample(sample) {
    design.empty();
    let block = $('<div></div>');
    block.append(formatInfo('Max Power', sample.getPower(), 'FE/t'));
    block.append(formatInfo('Avg Power', sample.getAvgPower(), 'FE/t'));
    block.append(formatInfoGT('Max Power (GT)', sample.getPower() / 8192));
    block.append(formatInfoGT('Avg Power (GT)', sample.getAvgPower() / 8192));
    block.append(formatInfo('Heat', sample.getHeat(), 'H/t'));
    block.append(formatInfo('Cooling', sample.getCooling(), 'H/t'));
    block.append(formatInfo('Net Heat', sample.getNetHeat(), 'H/t'));
    block.append(formatInfo('Duty Cycle', sample.getDutyCycle() * 100, '%'));
    block.append(formatInfo('Fuel Use Rate', sample.getAvgBreed(), '&times;'));
    block.append(formatInfo('Efficiency', sample.getEfficiency() * 100, '%'));
    design.append(block);

    const shapes = [], strides = [], data = sample.getData();
    for (let i = 0; i < 3; ++i) {
      shapes.push(sample.getShape(i));
      strides.push(sample.getStride(i));
    }
    let resourceMap = {};
    resourceMap[-1] = (shapes[0] * shapes[1] + shapes[1] * shapes[2] + shapes[2] * shapes[0]) * 2 + (shapes[0] + shapes[1] + shapes[2]) * 4 + 8;
    for (let y = 0; y < shapes[0]; ++y) {
      block = $('<div></div>');
      block.append('<div>Layer ' + (y + 1) + '</div>');
      for (let z = 0; z < shapes[1]; ++z) {
        const row = $('<div></div>').addClass('row');
        for (let x = 0; x < shapes[2]; ++x) {
          if (x)
            row.append(' ');
          const tile = data[y * strides[0] + z * strides[1] + x * strides[2]];
          if (!resourceMap.hasOwnProperty(tile))
            resourceMap[tile] = 1;
          else
            ++resourceMap[tile];
          row.append(displayTile(tile));
        }
        block.append(row);
      }
      design.append(block);
    }

    save.removeClass('disabledLink');
    save.off('click').click(async () => {
      let internalMap = {}
      let palette = {};
      let data = sample.getData();
      let internalIndex = 0;
      let blockData = new Int8Array(shapes[0] * shapes[1] * shapes[2]);
      for (let y = 0; y < shapes[0]; ++y) {
        for (let z = 0; z < shapes[1]; ++z) {
          for (let x = 0; x < shapes[2]; ++x) {
            const index = y * strides[0] + z * strides[1] + x * strides[2];
            const savedTile = saveTile(data[index]);
            if (!internalMap.hasOwnProperty(savedTile)) {
              palette[savedTile] = new Int32(internalIndex);
              blockData[index] = internalIndex;
              internalMap[savedTile] = internalIndex++;
            } else {
              blockData[index] = internalMap[savedTile];
            }
          }
        }
      }
      let res = await write({
        Width: new Int16(shapes[2]),
        Height: new Int16(shapes[0]),
        Length: new Int16(shapes[1]),
        Version: new Int32(2),
        DataVersion: new Int32(3465),
        PaletteMax: new Int32(Object.keys(palette).length),
        Palette: palette,
        BlockData: blockData
      });
      const elem = document.createElement('a');
      const url = window.URL.createObjectURL(new Blob([res]));
      elem.setAttribute('href', url);
      elem.setAttribute('download', 'reactor.schem');
      elem.click();
      window.URL.revokeObjectURL(url);
    });

    bgString.removeClass('disabledLink');
    bgString.off('click').click(async () => {
      let internalMap = {};
      let data = sample.getData();
      let internalIndex = 0;
      let stateList = [];
      for (let y = 0; y < shapes[1]; ++y) {
        for (let z = 0; z < shapes[0]; ++z) {
          for (let x = 0; x < shapes[2]; ++x) {
            const savedTile = `{Name:\\"${saveTile(data[z * shapes[2] * shapes[1] + y * shapes[2] + x])}\\"}`;
            if (!internalMap.hasOwnProperty(savedTile)) {
              stateList.push(internalIndex);
              internalMap[savedTile] = internalIndex++;
            } else {
              stateList.push(internalMap[savedTile]);
            }
          }
        }
      }
      let string = `{"statePosArrayList": "{blockstatemap:[${Object.keys(internalMap)}],startpos:{X:0,Y:0,Z:0},` +
          `endpos:{X:${shapes[2] - 1},Y:${shapes[0] - 1},Z:${shapes[1] - 1}},statelist:[I;${stateList}]}"}`;
      await navigator.clipboard.writeText(string);
    });

    block = $('<div></div>');
    block.append('<div>Total number of blocks used</div>')
    resourceMap = Object.entries(resourceMap);
    resourceMap.sort((x, y) => y[1] - x[1]);
    for (let resource of resourceMap) {
      if (parseInt(resource[0]) === AIR_ID) continue;
      const row = $('<div></div>');
      if (resource[0] < 0)
        row.append('Casing');
      else
        row.append(displayTile(resource[0]).addClass('row'));
      block.append(row.append(' &times; ' + resource[1]));
    }
    design.append(block);
  }
  function formatInfo(label, value, unit) {
    const row = $('<div></div>').addClass('info');
    row.append('<div>' + label + '</div>');
    row.append('<div>' + unit + '</div>');
    row.append(Math.round(value * 100) / 100);
    return row
  }

  function formatInfoGT(label, value) {
    const tiers = [
      {name: 'EV', divisor: 1},
      {name: 'IV', divisor: 4},
      {name: 'LuV', divisor: 16}
    ];
    let tier = 0;
    while (tier + 1 < tiers.length && value / tiers[tier + 1].divisor >= 16) {
      ++tier;
    }
    const shown = value / tiers[tier].divisor;
    const row = $('<div></div>').addClass('info');
    row.append('<div>' + label + '</div>');
    row.append('<div>A (' + tiers[tier].name + ')</div>');
    row.append(Math.round(shown * 100) / 100);
    return row
  }

  function step() {
    timeout = window.setTimeout(step, 0);
    opt.stepInteractive();
    const nStage = opt.getNStage();
    if (nStage === -2) {
      progress.text('Episode ' + opt.getNEpisode() + ', training iteration ' + opt.getNIteration());
    } else if (nStage === -1) {
      progress.text('Episode ' + opt.getNEpisode() + ', inference iteration ' + opt.getNIteration());
    } else {
      progress.text('Episode ' + opt.getNEpisode() + ', stage ' + nStage + ', iteration ' + opt.getNIteration());
    }

    if (opt.needsRedrawBest()) {
      displaySample(opt.getBest());
    }
    if (opt.needsReplotLoss()) {
      const data = opt.getLossHistory();
      while (lossPlot.data.labels.length < data.length)
        lossPlot.data.labels.push(lossPlot.data.labels.length);
      lossPlot.data.datasets[0].data = data;
      lossPlot.update({duration: 0});
    }
  }

  run.click(() => {
    if (timeout !== null)
      return;
    if (opt === null) {
      const parseSize = (x) => {
        const result = parseInt(x);
        if (!(result > 0))
          throw Error("Core size must be a positive integer");
        return result;
      };
      const parsePositiveFloat = (name, x) => {
        const result = parseFloat(x);
        if (!(result >= 0))
          throw Error(name + " must be a positive number");
        return result;
      };
      const parseLimits = (x) => {
        let result = parseInt(x)
        result = result >= 0 && !isNaN(result) ? result : -1;
        return result;
      };
      try {
        settings.sizeX = parseSize($('#sizeX').val());
        settings.sizeY = parseSize($('#sizeY').val());
        settings.sizeZ = parseSize($('#sizeZ').val());
        settings.fuelBasePower = parsePositiveFloat('Fuel Base Power', fuelBasePower.val());
        settings.fuelBaseHeat = parsePositiveFloat('Fuel Base Heat', fuelBaseHeat.val());
        settings.ensureHeatNeutral = $('#ensureHeatNeutral').is(':checked');
        settings.goal = parseInt($('input[name=goal]:checked').val());
        settings.symX = $('#symX').is(':checked');
        settings.symY = $('#symY').is(':checked');
        settings.symZ = $('#symZ').is(':checked');
        settings.genMult = parsePositiveFloat('Generation Multiplier', $('#genMult').val());
        settings.heatMult = parsePositiveFloat('Heat Multiplier', $('#heatMult').val());
        settings.modFEMult = parsePositiveFloat('Moderator FE Multiplier', $('#modFEMult').val());
        settings.modHeatMult = parsePositiveFloat('Moderator Heat Multiplier', $('#modHeatMult').val());
        settings.FEGenMult = parsePositiveFloat('FE Generation Multiplier', $('#FEGenMult').val());

        for (let i = 0; i < N_HEATSINKS; ++i) {
          settings.setLimit(i, 0);
          settings.setRate(i, 0);
        }

        let rateIndex = 0;
        for (let i = 0; i < COMPONENTS.length; ++i) {
          const component = COMPONENTS[i];
          const limit = parseLimits(limits[i].val());
          if (component.cooling_rate) {
            settings.setLimit(i, limit);
            settings.setRate(i, parsePositiveFloat('Cooling Rate', rates[rateIndex].val()));
            ++rateIndex;
          } else if (i === CELL_ID || i === MODERATOR_ID) {
            settings.setLimit(i, limit);
          }
        }
      } catch (error) {
        alert('Error: ' + error.message);
        return;
      }
      design.empty();
      save.off('click');
      save.addClass('disabledLink');
      if (lossElement !== undefined) lossElement.remove();
      const useNet = $('#useNet').is(':checked');
      if (useNet) {
        lossElement = $('<canvas></canvas>').attr('width', 1024).attr('height', 128).insertAfter(progress);
        lossPlot = new Chart(lossElement[0].getContext('2d'), {
          type: 'bar',
          options: {responsive: false, animation: {duration: 0}, hover: {animationDuration: 0}, scales: {xAxes: [{display: false}]}, legend: {display: false}},
          data: {labels: [], datasets: [{label: 'Loss', backgroundColor: 'red', data: [], categoryPercentage: 1.0, barPercentage: 1.0}]}
        });
      }
      opt = new FissionOpt.FissionOpt(settings, useNet);
    }
    timeout = window.setTimeout(step, 0);
    updateDisables();
  });
  pause.click(() => {
    if (timeout === null)
      return;
    window.clearTimeout(timeout);
    timeout = null;
    updateDisables();
  });
  stop.click(() => {
    if (opt === null)
      return;
    if (timeout !== null) {
      window.clearTimeout(timeout);
      timeout = null;
    }
    opt.delete();
    opt = null;
    updateDisables();
  })
})});
