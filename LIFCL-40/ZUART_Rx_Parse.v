//iXXX: input signal.
//oXXX: output signal.
//xxxN: negative signal.
`timescale 1ns/1ps

module ZUART_Rx_Parse
(
	input wire iClk,
	input wire iRstN,
	input wire iEn,


	input wire iDataValid,
	input wire[7:0] iData,

	output wire oCheckPassed
);

reg regCheckPassed;
assign oCheckPassed=regCheckPassed;

//driven by FSM.
reg [7:0] cntFSM;
reg [7:0] cntTimeout;
always @(posedge iClk or negedge iRstN)
if(!iRstN) begin
	cntFSM<=0; cntTimeout<=0; regCheckPassed<=0; 
end
else begin
	if(iEn) begin
		case(cntFSM)
			0: //Check 1st byte.
				if(iDataValid && iData==8'h19) begin cntFSM<=cntFSM+1; end
			1: //Check 2nd byte.
				if(iDataValid && iData==8'h87) begin cntTimeout<=0; cntFSM<=cntFSM+1; end
				else begin //timeout process here to return step-0.
					if(cntTimeout==8'hFF-1) begin cntTimeout<=0; cntFSM<=0; end
					else begin cntTimeout<=cntTimeout+1; end
				end
			2: //Check 3rd byte.
				if(iDataValid && iData==8'h09) begin cntTimeout<=0; cntFSM<=cntFSM+1; end
				else begin //timeout process here to return step-0.
					if(cntTimeout==8'hFF-1) begin cntTimeout<=0; cntFSM<=0; end
					else begin cntTimeout<=cntTimeout+1; end
				end
			3: //Check 4th byte.
				if(iDataValid && iData==8'h01) begin cntTimeout<=0; cntFSM<=cntFSM+1; end
				else begin //timeout process here to return step-0.
					if(cntTimeout==8'hFF-1) begin cntTimeout<=0; cntFSM<=0; end
					else begin cntTimeout<=cntTimeout+1; end
				end
			4: 
				begin regCheckPassed<=1; cntFSM<=cntFSM+1; end
			5:
				begin regCheckPassed<=0; cntFSM<=0; end
			default:
				begin regCheckPassed<=0; cntFSM<=0; end
		endcase
	end
	else begin
			cntFSM<=0; regCheckPassed<=0; 
		end
end
endmodule