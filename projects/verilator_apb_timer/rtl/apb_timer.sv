// ----------------------------------------------------------------------------
// apb_timer.sv — Synthesizable APB3 Programmable Down-Counter Timer
//
// Register map (32-bit word-aligned offsets):
//   0x00 CTRL       [RW]  [0] en, [1] periodic, [2] irq_en, [5:4] prescale
//   0x04 LOAD       [RW]  [7:0] reload value (reset 0x0F; writes while !en also
//                         update COUNT)
//   0x08 COUNT      [RO]  [7:0] live down-counter value
//   0x0C IRQ_STATUS [W1C] [0] irq_pending (set on terminal count 1->0 while en;
//                         cleared by writing 1 to bit 0)
//   0x10 SCRATCH    [RW]  [7:0] general-purpose scratchpad register
//
// Optional compile-time fault injection (-DBUG_SEED=1|2|3):
//   1: W1C defect — writing IRQ_STATUS overwrites bit 0 instead of W1C clearing
//   2: Prescaler off-by-one — prescale==2 (/4) divides by 3 instead of 4
//   3: Reload defect — periodic wrap reloads (load_q - 1) instead of load_q
// ----------------------------------------------------------------------------
`ifndef BUG_SEED
`define BUG_SEED 0
`endif

module apb_timer (
  input  wire        PCLK,
  input  wire        PRESETn,
  input  wire        PSEL,
  input  wire        PENABLE,
  input  wire        PWRITE,
  input  wire [31:0] PADDR,
  input  wire [31:0] PWDATA,
  output reg  [31:0] PRDATA,
  output wire        PREADY,
  output reg         PSLVERR,
  output wire        IRQ
);

  reg [7:0] ctrl_q       /*verilator public_flat_rw*/;
  reg [7:0] load_q       /*verilator public_flat_rw*/;
  reg [7:0] count_q      /*verilator public_flat_rw*/;
  reg       irq_status_q /*verilator public_flat_rw*/;
  reg [7:0] scratch_q    /*verilator public_flat_rw*/;
  reg [2:0] pre_cnt_q;

  assign PREADY = 1'b1;

  wire       cfg_en       = ctrl_q[0];
  wire       cfg_periodic = ctrl_q[1];
  wire       cfg_irq_en   = ctrl_q[2];
  wire [1:0] cfg_prescale = ctrl_q[5:4];

  assign IRQ = cfg_irq_en & irq_status_q;

  wire apb_setup_or_access = PSEL && PENABLE;
  wire addr_aligned        = (PADDR[1:0] == 2'b00);
  wire addr_valid          = addr_aligned && (PADDR <= 32'h0000_0010);

  reg [2:0] pre_limit;
  always @(*) begin
    case (cfg_prescale)
      2'd0: pre_limit = 3'd0; // /1
      2'd1: pre_limit = 3'd1; // /2
`ifdef BUG_SEED
      2'd2: pre_limit = (`BUG_SEED == 2) ? 3'd2 : 3'd3; // /4 (bug: /3)
`else
      2'd2: pre_limit = 3'd3;
`endif
      2'd3: pre_limit = 3'd7; // /8
    endcase
  end

  wire tick = cfg_en && (pre_cnt_q == pre_limit);

  always @(posedge PCLK or negedge PRESETn) begin
    if (!PRESETn) begin
      ctrl_q       <= 8'h00;
      load_q       <= 8'h0F;
      count_q      <= 8'h0F;
      irq_status_q <= 1'b0;
      scratch_q    <= 8'h00;
      pre_cnt_q    <= 3'd0;
      PRDATA       <= 32'h0000_0000;
      PSLVERR      <= 1'b0;
    end else begin
      PSLVERR <= 1'b0;

      // Default timer progression before APB write overrides
      if (cfg_en) begin
        if (pre_cnt_q == pre_limit) begin
          pre_cnt_q <= 3'd0;
          if (count_q == 8'd1) begin
            irq_status_q <= 1'b1;
            if (cfg_periodic) begin
              count_q <= (`BUG_SEED == 3) ? (load_q - 8'd1) : load_q;
            end else begin
              count_q   <= 8'd0;
              ctrl_q[0] <= 1'b0;
            end
          end else if (count_q == 8'd0) begin
            if (cfg_periodic) begin
              count_q <= (`BUG_SEED == 3) ? (load_q - 8'd1) : load_q;
            end else begin
              ctrl_q[0] <= 1'b0;
            end
          end else begin
            count_q <= count_q - 8'd1;
          end
        end else begin
          pre_cnt_q <= pre_cnt_q + 3'd1;
        end
      end else begin
        pre_cnt_q <= 3'd0;
      end

      // APB3 access phase
      if (apb_setup_or_access) begin
        if (!addr_valid) begin
          PSLVERR <= 1'b1;
          PRDATA  <= 32'h0000_0000;
        end else if (PWRITE) begin
          case (PADDR[7:0])
            8'h00: begin
              ctrl_q    <= PWDATA[7:0] & 8'h37;
              pre_cnt_q <= 3'd0;
            end
            8'h04: begin
              load_q <= PWDATA[7:0];
              if (!cfg_en)
                count_q <= PWDATA[7:0];
            end
            8'h08: begin
              // COUNT is read-only; writes are ignored
            end
            8'h0C: begin
              if (`BUG_SEED == 1)
                irq_status_q <= PWDATA[0];
              else if (PWDATA[0])
                irq_status_q <= (tick && (count_q == 8'd1)) ? 1'b1 : 1'b0;
            end
            8'h10: begin
              scratch_q <= PWDATA[7:0];
            end
            default: begin
              PSLVERR <= 1'b1;
            end
          endcase
        end else begin
          case (PADDR[7:0])
            8'h00:   PRDATA <= {24'h0, ctrl_q};
            8'h04:   PRDATA <= {24'h0, load_q};
            8'h08:   PRDATA <= {24'h0, count_q};
            8'h0C:   PRDATA <= {31'h0, irq_status_q};
            8'h10:   PRDATA <= {24'h0, scratch_q};
            default: PRDATA <= 32'h0;
          endcase
        end
      end
    end
  end

endmodule
