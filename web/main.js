$(() => { FissionOpt().then((FissionOpt) => {
  const run = $('#run'), pause = $('#pause'), stop = $('#stop');
  let opt = null, timeout = null;

  const updateDisables = () => {
    $('#settings input').prop('disabled', opt !== null);
    $('#settings a')[opt === null ? 'removeClass' : 'addClass']('disabledLink');
    run[timeout === null ? 'removeClass' : 'addClass']('disabledLink');
    pause[timeout !== null ? 'removeClass' : 'addClass']('disabledLink');
    stop[opt !== null ? 'removeClass' : 'addClass']('disabledLink');
  };

  const fuelBasePower = $('#fuelBasePower');
  const fuelBaseHeat = $('#fuelBaseHeat');
  const fuelPresets = {
    TBU: [60*80, 18],
    LEU235: [120*80, 50],
    HEU235: [480*80, 300],
    LEU233: [144*80, 60],
    HEU233: [576*80, 360],
    LEN236: [90*80, 36],
    HEN236: [360*80, 216],
    LEP239: [105*80, 40],
    HEP239: [420*80, 240],
    LEP241: [165*80, 70],
    HEP241: [660*80, 420],
    LEA242: [192*80, 94],
    HEA242: [768*80, 564],
    LECm243: [210*80, 112],
    HECm243: [840*80, 672],
    LECm245: [162*80, 68],
    HECm245: [648*80, 408],
    LECm247: [138*80, 54],
    HECm247: [552*80, 324],
    LEB248: [135*80, 52],
    HEB248: [540*80, 312],
    LECf249: [216*80, 116],
    HECf249: [864*80, 696],
    LECf251: [225*80, 120],
    HECf251: [900*80, 720],
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
  let limits = []
  $('#rate input').each(function() { rates.push($(this)); });
  $('#activeRate input').each(function() { rates.push($(this)); });
  $('#limit input').each(function() { limits.push($(this)); });
  {
    const tail = limits.splice(-2);
    $('#activeLimit input').each(function() { limits.push($(this)); });
    limits.push(...tail);
  }

  $("#temperatureList").on("change", function() {$("#temperature").val(this.value)});
  $("#modFEMult").val(16.67)
  $("#modHeatMult").val(33.34)

  const schedule = () => {
    timeout = window.setTimeout(step, 0);
  }

  const settings = new FissionOpt.FissionSettings();
  const design = $('#design');
  const save = $('#save');
  const bgString = $('#bgString');
  const nCoolerTypes = 31, air = nCoolerTypes * 2 + 2;
  const tileNames = ['Wt', 'Rs', 'He', 'Ed', 'Cr', 'Nt', 'Qz', 'Au', 'Gs', 'Lp', 'Dm', 'Fe', 'Em', 'Cu', 'Sn', 'Mg', 'Mn', 'En', 'As', 'Pm', 'Ob', 'Al', 'Vi', 'Bo', 'Ag', 'Fl', 'Nr', 'Pb', 'Pr', 'Sm', 'Li', '[]', '##', '..'];
  const tileTitles = ['Water', 'Redstone', 'Liquid Helium', 'Enderium', 'Cryotheum', 'Liquid Nitrogen', 'Quartz', 'Gold', 'Glowstone', 'Lapis', 'Diamond',
    'Iron', 'Emerald', 'Copper', 'Tin', 'Magnesium', 'Manganese', 'End Stone', 'Arsenic', 'Prismarine', 'Obsidian', 'Aluminum',
    'Villiaumite', 'Boron', 'Silver', 'Fluorite', 'Nether Brick', 'Lead', 'Purpur', 'Slime', 'Lithium', 'Fuel Cell', 'Moderator', 'Air'];
  $('#blockType>:not(:first)').each((i, x) => { $(x).attr('title', tileTitles[i]); });
  const tileClasses = tileNames.slice();
  tileClasses[31] = 'cell';
  tileClasses[32] = 'mod';
  tileClasses[33] = 'air';
  const tileSaveNames = tileTitles.slice(0, 32);
  tileSaveNames[31] = 'fission_reactor_solid_fuel_cell';
  tileSaveNames[32] = 'graphite_block';

  const displayTile = (tile) => {
    let active = false;
    if (tile >= nCoolerTypes) {
      tile -= nCoolerTypes;
      if (tile < nCoolerTypes)
        active = true;
    }
    const result = $('<span>' + tileNames[tile] + '</span>').addClass(tileClasses[tile]);
    if (active) {
      result.attr('title', 'Active ' + tileTitles[tile]);
      result.css('outline', '2px dashed black')
    } else {
      result.attr('title', tileTitles[tile]);
    }
    return result;
  };

  const saveTile = (tile) => {
    if (tile >= nCoolerTypes) {
      tile -= nCoolerTypes;
      if (tile < nCoolerTypes) {
        return "nuclearcraft:active_" + tileSaveNames[tile].toLowerCase().replaceAll(" ", "_") + "_heat_sink";
      } else {
        if (tile === tileTitles.length - 1) {
          return "minecraft:air";
        } else {
          return "nuclearcraft:" + tileSaveNames[tile];
        }
      }
    }
    return "nuclearcraft:" + tileSaveNames[tile].toLowerCase().replaceAll(" ", "_") + "_heat_sink";
  };

  const displaySample = (sample) => {
    design.empty();
    let block = $('<div></div>');
    const appendInfo = (label, value, unit) => {
      const row = $('<div></div>').addClass('info');
      row.append('<div>' + label + '</div>');
      row.append('<div>' + unit + '</div>');
      row.append(Math.round(value * 100) / 100);
      block.append(row);
    };
    appendInfo('Max Power', sample.getPower(), 'FE/t');
    appendInfo('Heat', sample.getHeat(), 'H/t');
    appendInfo('Cooling', sample.getCooling(), 'H/t');
    appendInfo('Net Heat', sample.getNetHeat(), 'H/t');
    appendInfo('Duty Cycle', sample.getDutyCycle() * 100, '%');
    appendInfo('Fuel Use Rate', sample.getAvgBreed(), '&times;');
    appendInfo('Efficiency', sample.getEfficiency() * 100, '%');
    appendInfo('Avg Power', sample.getAvgPower(), 'FE/t');
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
    save.off('click').click(() => {
      import("https://cdn.jsdelivr.net/npm/nbtify@2.2.0/+esm").then(async (NBT) => {
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
                palette[savedTile] = new NBT.Int32(internalIndex);
                blockData[index] = internalIndex;
                internalMap[savedTile] = internalIndex++;
              } else {
                blockData[index] = internalMap[savedTile];
              }
            }
          }
        }
        let res = await NBT.write({
          Width: new NBT.Int16(shapes[2]),
          Height: new NBT.Int16(shapes[0]),
          Length: new NBT.Int16(shapes[1]),
          Version: new NBT.Int32(2),
          DataVersion: new NBT.Int32(3465),
          PaletteMax: new NBT.Int32(Object.keys(palette).length),
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
    });

    bgString.removeClass('disabledLink');
    bgString.off('click').click(() => {
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
      navigator.clipboard.writeText(string);
    });

    block = $('<div></div>');
    block.append('<div>Total number of blocks used</div>')
    resourceMap = Object.entries(resourceMap);
    resourceMap.sort((x, y) => y[1] - x[1]);
    for (resource of resourceMap) {
      if (resource[0] === air)
        continue;
      const row = $('<div></div>');
      if (resource[0] < 0)
        row.append('Casing');
      else
        row.append(displayTile(resource[0]).addClass('row'));
      block.append(row.append(' &times; ' + resource[1]));
    }
    design.append(block);
  }

  const progress = $('#progress');
  let lossElement, lossPlot;

  function step() {
    schedule();
    opt.stepInteractive();
    const nStage = opt.getNStage();
    if (nStage === -2)
      progress.text('Episode ' + opt.getNEpisode() + ', training iteration ' + opt.getNIteration());
    else if (nStage === -1)
      progress.text('Episode ' + opt.getNEpisode() + ', inference iteration ' + opt.getNIteration());
    else
      progress.text('Episode ' + opt.getNEpisode() + ', stage ' + nStage + ', iteration ' + opt.getNIteration());
    if (opt.needsRedrawBest())
      displaySample(opt.getBest());
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
      const parseValidFloat = (name, x) => {
        const result = parseFloat(x);
        if (isNaN(parseFloat(x)))
          throw Error(name + " must be a number");
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
        settings.temperature = parseValidFloat('Temperature', $('#temperature').val());
        settings.genMult = parsePositiveFloat('Generation Multiplier', $('#genMult').val());
        settings.heatMult = parsePositiveFloat('Heat Multiplier', $('#heatMult').val());
        settings.modFEMult = parsePositiveFloat('Moderator FE Multiplier', $('#modFEMult').val());
        settings.modHeatMult = parsePositiveFloat('Moderator Heat Multiplier', $('#modHeatMult').val());
        settings.FEGenMult = parsePositiveFloat('FE Generation Multiplier', $('#FEGenMult').val());
        settings.activeHeatsinkPrime = $('#activeHeatsinkPrime').is(':checked');
        $.each(rates, (i, x) => { settings.setRate(i, parsePositiveFloat('Cooling Rate', x.val())); });
        $.each(limits, (i, x) => {
          x = parseInt(x.val());
          settings.setLimit(i, x >= 0 ? x : -1);
        });
      } catch (error) {
        alert('Error: ' + error.message);
        return;
      }
      design.empty();
      save.off('click');
      save.addClass('disabledLink');
      if (lossElement !== undefined)
        lossElement.remove();
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
    schedule();
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
